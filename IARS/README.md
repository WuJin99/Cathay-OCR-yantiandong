# IARS-yantiandong

面向岸桥/堆场作业的 OCR 采集服务。程序从 PLC 与上层系统接收作业状态、任务和控制消息，按相机配置抓拍图片并请求 CRS 识别服务，复合车号、箱号、ISO、箱门、车牌等结果，随后通过 ZMQ 与 PLC HTTP 接口上报。

## 代码使用说明

- 解压include.7z然后解压postmsg.7z

## 工程与运行

- 工程入口：`IARS/IRASmain.cpp`；Visual Studio 解决方案：`IARS.sln`。
- 目标：Windows x64 控制台应用，项目工具集为 `v141`；`x64/Release/IARS.exe` 为已有发布产物。
- 主配置：运行目录的 `AppConfig.xml`；车道相机/事件配置由 `LANESIDE/Lane@cfg` 指定（默认 `CameraConfig.xml`）。
- 主要依赖：海康 SDK、 大华 SDK、ICE 车牌 SDK、OpenCV、gRPC/Protobuf、ZeroMQ、Boost、spdlog、tinyxml 与可选 FTP。

运行时会在程序目录/配置目录使用或生成 `ConsoleLog/`（主日志）、`RePub/`（待确认消息落盘）、任务图片及任务流水日志；`savePath`、`imgRootPath` 等由 `USER` 配置控制。

## 总体链路

```text
AppConfig.xml + CameraConfig.xml
            │
            ▼
main → CRemoteControl::Start
  ├─ PicSave：图片保存/FTP 补传
  ├─ ZMQManager：上行 XPUB、下行 SUB、心跳、确认与重发
  ├─ 每个 LaneSide → TaskManager
  │    └─ CDeviceManager → 海康/大华/Axis/ICE 相机
  ├─ CGRPCRequest：连接 CRS 识别服务
  └─ PLCHttpComm：PLC HTTP 读写

PLC 状态/JobTrigger → TaskManager::TaskTrigger
  → PLCInfoState 更新、Event 条件匹配
  → 相机 Action(PTZ/抓拍) → RRIMAGELIST
  → CGRPCRequest::GetOcrResult → ComposeResult
  → CDataIntegration → OcrData / TaskInfo
  → ZMQ OCRAny 上报 + PLCHttpComm 结果写入
  → PicSave 本地保存，FTP 可选补传
```

`main()` 还负责单实例互斥、崩溃转储处理、日志初始化，以及按间隔从 `g_StateInfo` 组装设备状态消息。退出时停止远程控制模块。

## 启动与消息链路

1. `LoadAppConfigXML()` 读取 broker、CRS、PLC HTTP、FTP、监控阈值、车道及箱号/ISO 修正规则。
2. `CRemoteControl::Start()` 启动图片服务、ZMQ、每条车道的 `TaskManager`、调试模块、额外 SUB、CRS 客户端、PLC 数据记录和 PLC HTTP 服务。
3. `ZMQManager::Start()` 建立 SUB 与 XPUB，首先发送心跳和 `TruckListRequest`；后台循环处理下行消息、上行队列、状态信息及心跳。
4. 下行 `THPMResult`（车顶号）和 `TRUCKSTATUS` 分发给全部车道的 `TaskManager`；`JobTrigger` 则建立/更新作业并据任务内容筛选可用相机。
5. PLC 状态变化被解析为 `PLCInfo`，再由 `PLCInfoState` 提供“闭锁/开锁、吊具升降、运行方向、车道进离、阈值穿越”等边沿判断。`TaskManager::TaskTrigger()` 根据这些状态驱动作业事件。
6. 相机将事件对应的原图与识别区域构造成 `RRIMAGELIST`。`SerialSingleRTaskProto()` 请求 CRS；识别结果先经 `ComposeResult` 校验/归并，再经 `CDataIntegration` 汇总到标准 `OcrData`。
7. `SendResult()`、`SendTaskInfo()` 生成 Protobuf `OCRAny` 放入 `g_WaitPubMsg`，由 ZMQ 发往上层；箱门方向、箱门开启、心跳与系统状态可同时由 `PLCHttpComm` 写入 PLC 接口。

## 关键模块与函数

| 模块 | 关键函数 | 职责 |
| --- | --- | --- |
| `IRASmain.cpp` | `main`、`CtrlHandler` | 进程生命周期、单实例、配置/日志/异常处理、状态信息定时上报。 |
| `AppConfig` | `LoadAppConfigXML` | 解析应用 XML 到全局 `g_AppConfig`。 |
| `CameraConfig` | `Config::load`、`EventCond::Satisfied`、`Event::Triggered` | 读取相机、位置、车道和事件配置；按 PLC 当前/前一状态判断事件触发。 |
| `CRemoteControl` | `Start`、`Stop` | 统一编排通信、任务、识别、PLC、图片和调试子系统。 |
| `ZMQManager` | `DealSub`、`DealXPub`、`DealStateInfo` | 分发下行 Protobuf；管理需确认消息、落盘、超时重发/删除；采集资源状态。 |
| `CZmqComm` | `Start`、`SubThread`、`XPubThread`、`ParseMsg` | ZeroMQ socket 与线程封装，传输/反序列化 Protobuf。 |
| `TaskManager` | `TaskTrigger`、`InitTaskData`、`DealEvent*`、`SerialSingleRTaskProto`、`SendResult` | 每车道作业状态机；接收 PLC 事件、调度相机/识别、维护作业数据并上报结果。 |
| `PLCInfo` / `PLCInfoState` | `ParseMsg`、`Update`、`isLockOccur` 等 | PLC 报文解析及状态边沿、阈值穿越、方向/车道等语义判断。 |
| `CDeviceManager` | `Start`、`LoadDevCfg`、SDK 回调 | 创建并监控海康/大华/Axis/ICE 设备，处理连接与图片回调。 |
| 相机类 | `CHKGun::Action`、`CDHCam::Action`、`IPCamera::Action`、`MakeRImage` | PTZ 定位、抓拍、ROI/旋转/灰度预处理、构造 CRS 请求图片。 |
| `CGRPCRequest` | `Start`、`ReConnect`、`GetOcrResult`、`RecordRoute` | CRS 主/备连接、身份认证、超时控制和批量图片识别请求。 |
| `ComposeResult` | `ComposeResults`、`ComposeCNR`、`ComposePMR`、`ISO6346Check` | 按识别类型分派、车号/箱号/ISO/箱门/残损等结果校验与复合。 |
| `CDataIntegration` | `DataIntegration` | 将已复合的图像结果按作业、车道和箱组整合为 `OcrData`。 |
| `PLCHttpComm` | `Start`、`SendDoorDirData`、`SendDoorOpenData` | PLC HTTP 服务/客户端通信及 OCR 结果状态写入。 |
| `PicSave` / `CFtpClient` | `ProcessPic`、`ReSaveToFtp`、`UploadWork` | 标注结果图落盘，失败图片队列化并按配置补传 FTP。 |
| `Monitor` / `Utils` | `GetCpuRate`、`PackStateInfo`、`WriteProtoToLocalFile` | 资源监控、状态对象封装、文件/时间/图像/协议等公共能力。 |

## 作业状态机要点

`TaskManager` 的事件集合覆盖集卡到位/离开、吊具着箱/离箱、闭锁/开锁、空吊具升降、带箱升降与移动、任务结束。事件处理函数会：

- 建立或清理任务目录、任务号、识别缓存和日志；
- 调用相机的配置事件，完成 PTZ、抓拍与识别请求；
- 合并内外集卡车顶号、车牌、箱号与 ISO，支持计划车号比对和可选模糊匹配；
- 累计箱门方向/开启结果，并按置信度与次数阈值决定是否写 PLC；
- 在开锁、离开或任务结束等节点发送作业事件与最终结果。

结果错误码在 `TaskManager` 中定义：覆盖车号/箱号/ISO/箱门方向/箱门开启的在线超时、离线超时、计划不匹配和低置信度。`ComposeResult` 同时支持从 `OWNERNUMBER`、`ISONUMBER` 映射纠正识别字符，并对箱号做 ISO 6346 校验位验证。

## 配置重点

`AppConfig.xml` 中常用节点：

- `PLC/Http`：PLC 服务地址、端口及车号、箱号、箱门接口路径；当前代码使用 `PLCHttpComm`。
- `BROKER`：XPUB/SUB 地址、端口和确认消息的最大重发次数。
- `SUB`：额外订阅源；接收内容进入全局下行队列。
- `CRS/item`：识别服务主/备地址；客户端按配置尝试重连。
- `USER`：岸桥编号、IARS 名称、图片路径、心跳/状态周期、确认重发超时、PTZ 等待和结果阈值。
- `LANESIDE/Lane`：车道名称、`lane_id` 与相机配置文件。
- `MONITOR`：CPU/GPU/内存/磁盘阈值；越限被编码为状态告警。

相机配置中的 `Event` 定义 PLC 条件、PTZ、抓拍识别参数 (`RecogParam`：识别类别、ROI、旋转、反色/矫正) 和联动步骤。修改事件条件前，应核对 `PLCInfoState` 中对应字段的边沿语义，避免静态状态被重复触发。

## 可靠性与排查

- 需要确认的 ZMQ 上行消息会写入 `RePub/<msg_id>`；收到成功确认后删除，超时按 `needReplyResend`/`resend_count` 重发或丢弃。
- ZMQ 心跳超时会将服务端标为不可用；此时非必要应答的消息不再发送，防止 broker 堆积。
- CRS 连接状态由 `CGRPCRequest` 维护，失败时依次尝试配置中的主/备服务；状态会反映到系统状态信息。
- 相机 SDK 的断线/重连回调由 `CDeviceManager` 处理，并通过 `PackStateInfo` 汇总健康状态。
- 主日志在 `ConsoleLog`，每作业流水在任务目录的 `*_flow.txt`；PLC 原始数据记录由 `CRecodData` 管理。

## 维护入口建议

1. 新增 PLC 触发：先扩展 `PLCInfo::ParseMsg`/`PLCInfoState` 的语义判断，再在 `CameraConfig` 中配置并由 `TaskManager::TaskTrigger` 消费。
2. 新增相机或识别区域：补充配置类/设备类，在 `CDeviceManager` 注册，并确保 `MakeRImage` 设置正确的图像标签和 `RecogParam`。
3. 调整结果规则：优先定位 `ComposeResult` 的对应 `Compose*R` 函数；最终上报字段整合在 `CDataIntegration` 和 `TaskManager::SendResult`。
4. 调整消息协议：检查 `include/protomsg` 中的 Protobuf 定义及 `ZMQManager` 的确认、落盘和重发逻辑。
