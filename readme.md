# OCR-IARS 软件架构与模块说明

## 动态加载库，不同模块的依赖文件放在不同的文件夹，方便管理windows和linux

- 当前项目的动态库机制是“主程序按 XML 顺序加载业务插件”，但发布结构仍是单目录 DLL 堆放，尚不具备可靠的跨平台分模块依赖管理能力。

```txt
IARS.exe
  └─ AppConfig.xml: path="WORKFLOWDLL.dll"
       └─ LoadLibrary("WORKFLOWDLL.dll")
            └─ 依赖 DLL 主要依靠当前工作目录 / .\lib / 系统搜索路径
```

- 核心依据：
  - 主程序读取 <item name / cfg / path / enable>，再直接以 module.path 调用 LoadLibrary；模块以 start / stop / version 三个导出函数作为插件接口。[AppConfig.cpp (line 17)](/D:/svn/IARS/OCR-IARS/IARS/AppConfig.cpp:17) [IARSmain.cpp (line 116)](/D:/svn/IARS/OCR-IARS/IARS/IARSmain.cpp:116)
  - Debug 配置中的模块路径全是裸 DLL 文件名，例如 WORKFLOWDLL.dll、CameraUNV.dll，没有模块目录或平台目录。[AppConfig.xml (line 4)](/D:/svn/IARS/OCR-IARS/x64/Debug/AppConfig.xml:4)
  - 程序调用 SetDllDirectory(".\\lib")，但当前可见发布依赖主要放在 x64\Release / x64\Debug 根目录，目录约定与代码不一致。[IARSmain.cpp (line 246)](/D:/svn/IARS/OCR-IARS/IARS/IARSmain.cpp:246)
- 主要问题：
  1. 依赖隔离不足
     1. OpenCV、海康/宇视 SDK、FFmpeg、ZMQ 等 DLL 在输出根目录平铺。不同模块若依赖同名但版本不同的 DLL，会发生“先加载者决定全进程版本”的冲突；模块难以独立升级、回滚或裁剪。
  2. 相对路径依赖当前工作目录
     1. path、cfg 都直接使用字符串，cfg 并非相对 AppConfig.xml 所在位置解析。通过服务、快捷方式、IDE 或其他工作目录启动时，模块及配置都可能找不到。
  3. Windows 的加载目录策略不完整
     1. SetDllDirectory 只能设置一个目录，且不能表达“每个模块自己的依赖目录”。业务模块在子目录时，其二级依赖也不会自然从任意 deps 子目录加载；不能依赖 PATH 解决，既不稳定也有 DLL 劫持风险。
  4. Linux 分支目前只是未完成的条件编译
     1. Linux 分支使用了不存在的 module.module_path，而失败处理仍调用 GetLastError 和_com_error；头文件、主流程还依赖 Windows API。因此当前代码并不能直接在 Linux 构建运行。[IARSmain.cpp (line 121)](/D:/svn/IARS/OCR-IARS/IARS/IARSmain.cpp:121)
     2. 同时项目根目录没有自身的 CMake/Makefile 跨平台构建入口，现有工程主要是 Visual Studio .sln/.vcxproj。

- 建议的运行包布局如下，按“业务模块私有依赖 + 公共运行库”组织：

```txt
IARS/
├─ bin/
│  └─ IARS.exe / iars
├─ config/
│  ├─ AppConfig.xml
│  └─ modules/
├─ modules/
│  ├─ workflow/
│  │  ├─ bin/       WORKFLOWDLL.dll / libworkflow.so
│  │  └─ lib/       仅 workflow 私有依赖
│  ├─ camera-hk/
│  │  ├─ bin/       CameraHK.dll / libcamera_hk.so
│  │  └─ lib/       海康 SDK 及其私有依赖
│  ├─ camera-unv/
│  │  ├─ bin/
│  │  └─ lib/
│  └─ ...
├─ runtime/
│  └─ lib/           全模块严格共用、版本统一的库，如 protobuf/zmq
└─ var/
   ├─ log/
   ├─ data/
   └─ config-backup/
```

- 落地原则：
  - XML 不应直接写当前目录下的裸 DLL 名；使用模块 ID 或以安装根目录为基准的相对路径，例如 modules/workflow/bin/WORKFLOWDLL.dll。配置文件路径也同样以 config/ 或模块目录为基准解析。
  - Windows：主程序先调用 SetDefaultDllDirectories，随后通过 AddDllDirectory 注册 runtime/lib 和该模块的 lib，使用绝对路径 LoadLibraryExW 加载模块，并采用 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS。不要依赖进程 PATH。
  - Linux：插件使用绝对路径 dlopen(..., RTLD_NOW | RTLD_LOCAL)；构建阶段为主程序/每个 .so 写入相对 RUNPATH，如 $ORIGIN/../lib。不要把 LD_LIBRARY_PATH 当成正式发布方案；它仅适合调试或临时覆盖。
  - 每个模块若依赖不同版本的第三方库，最好让其私有依赖保持唯一 SONAME/文件名，或使用容器/进程隔离；同名共享库的“目录隔离”在一个进程内不能完全解决版本冲突。
  - 模块 ABI 当前使用 std::string 作为跨 DLL 边界参数和返回值。[AppConfig.h (line 15)](/D:/svn/IARS/OCR-IARS/IARS/AppConfig.h:15) 这要求所有模块在 Windows/Linux、编译器、C++ 标准库和运行时版本上保持一致。若目标是长期跨平台插件体系，建议改为 C ABI，例如 extern "C"、const char*、固定长度缓冲区或显式内存释放函数。

- 结论
  - 可以按模块分目录放置依赖，但需要同时改造“路径解析、Windows 搜索目录、Linux RUNPATH、ABI 和构建安装规则”。仅把 DLL/SO 移进不同文件夹而保持现有 LoadLibrary("xxx.dll")，并不能实现可靠管理。

## 修改方案

- 必须改：主程序加载逻辑
IARSmain.cpp 需要从现在的裸 LoadLibrary(module.path) 改为：
先获取“可执行文件所在目录”，不再依赖当前工作目录；
基于模块 ID 计算模块根目录、插件文件和配置文件的绝对路径；
Windows/Linux 分别封装加载、符号查找、卸载、错误信息；
Windows 注册公共库目录和当前模块私有库目录；
Linux 用 dlopen + dlerror，并配合构建期 RPATH；
加载失败日志应记录：插件绝对路径、实际搜索目录、底层加载器错误、缺失依赖名称。
现在 Linux 分支不仅功能不完整，代码本身也有明显问题：module.module_path 并不存在，失败处理仍使用 Windows 的 GetLastError/_com_error。这部分必须改成统一平台抽象。
必须改：配置模型与路径规则
ModuleConfig 至少应区分：
模块 ID
是否启用
模块配置文件
插件主库
模块私有库目录
更理想的是 XML 只维护逻辑名，而不写平台 DLL/SO 文件名：
<item id="workflow" enable="true" config="CameraConfig.xml"/>

<item id="camera-unv" enable="true" config="CameraConfig.xml"/>
加载器根据运行平台自行找到：
modules/workflow/bin/WORKFLOWDLL.dll       Windows
modules/workflow/bin/libworkflow.so        Linux
这样避免一份配置同时混杂 .dll、.so、Debug/Release、x64/arm64 等部署细节。
同时，模块配置不应再相对工作目录解析，而应明确相对 config/ 或相对模块目录解析。
强烈建议改：插件 ABI
当前动态接口是：
int start(std::string);
int stop();
std::string version();
这在同一套 MSVC 工具链下通常可以工作，但不适合把 Windows/Linux 或不同编译器、不同 C++ 运行时的模块真正独立管理。std::string 跨动态库边界涉及 C++ ABI 和内存释放责任。
建议逐步收敛为 C ABI，例如：
extern "C" {
  int module_start(const char* config_path);
  int module_stop();
  const char* module_version();
}
或提供统一的 ModuleApiV1 函数表，并加入：
ABI 版本；
模块名称和版本；
初始化/停止；
可选能力标志；
错误信息获取接口。
这能让主程序在加载前校验插件是否兼容，避免“成功加载但符号或 ABI 不兼容”。
不必一次性改所有业务代码。较稳妥的顺序是：
先实现跨平台 DynamicLibrary/ModuleLoader 公共类；
改 IARS 主程序与 XML 路径规则；
保持旧 start/stop/version 暂时兼容；
新模块开始使用 C ABI / API 版本；
按模块逐步把 SDK、OpenCV、FFmpeg 等依赖迁到模块私有目录；
最后再统一 CMake 安装、RPATH 与 Windows 发布脚本。

## 改cmake建议

难度中等偏大，但跨平台目标下，建议改为 CMake；不建议直接“放弃 Visual Studio”。
CMake 可以生成 Visual Studio 解决方案，因此 Windows 仍可在 VS 中打开、调试、编译。变化只是从维护十多个 .vcxproj，改为维护根目录和各模块的 CMakeLists.txt。
当前项目迁移难点主要不在 C++ 源码，而在构建配置：
项目较多：IARS、各业务 DLL、Camera/PLC/Workflow 等都要转换为独立 target。
依赖复杂：OpenCV、protobuf/grpc、ZMQ、海康/宇视 SDK、FFmpeg、Halcon、Boost 等，需要区分头文件、导入库、运行时 DLL/SO。
现有 VS 工程中有大量 Windows 专属配置：#pragma comment(lib)、.lib 路径、__declspec、MSVC runtime、预处理宏、x64/Debug/Release 设置。
第三方厂商 SDK 未必有 Linux 版本或一致 API；这才是跨平台真正的大头。即使 CMake 完成，CameraHK、CameraUNV、PLC 或 HALCON 的 Linux SDK、授权与二进制兼容性仍需单独验证。
现有动态加载及运行路径本身还需跨平台改造；CMake 负责“构建和安装”，不能自动使 Windows API 变成 Linux 可用。
是否需要改成 CMake：
只维护 Windows，且短期不扩展部署方式：不强制。继续使用 VS 工程也能完成 DLL 分目录发布，只是发布脚本和依赖管理会越来越难维护。
需要 Windows + Linux 都能构建、测试、打包：基本应该使用 CMake。
只想在 Linux 运行某些模块：推荐 CMake，但可以先迁移公共库、主程序、Linux 可用模块；Windows 专属模块保留为条件 target。
建议采用渐进迁移，不要一次性重写：
阶段 1：建立根 CMake，先生成 IARS.exe 和 CommonTools
阶段 2：迁移无厂商 SDK 的业务模块 DLL
阶段 3：引入统一的第三方依赖定义与 Debug/Release、x64/arm64 配置
阶段 4：按平台接入海康、宇视、PLC、HALCON 等 SDK
阶段 5：加入 install/package 规则，生成分模块运行包
阶段 6：以 CMake 生成 Visual Studio 工程，旧 .sln 逐步退出
最终目录可保持 VS 使用习惯：
OCR-IARS/
├─ CMakeLists.txt
├─ cmake/              # 第三方 SDK、平台配置、安装规则
├─ IARS/
│  └─ CMakeLists.txt
├─ CommonTools/
│  └─ CMakeLists.txt
├─ CameraHK/
│  └─ CMakeLists.txt
└─ ...
关键点是：CMake 不是为了“替代 Visual Studio”，而是把构建、依赖路径、DLL/SO 安装位置、RPATH 等规则变成跨平台且可复现的配置；Visual Studio 仍然是 Windows 开发和调试工具。
以该项目规模，先让 Windows 的 CMake 构建与现有 Release 输出完全一致，通常是可控的第一步；真正耗时的是逐个确认并接入 Linux 版第三方 SDK，而不是写 CMake 文件

## 1. 项目概述

OCR-IARS 是一个面向岸桥/场桥 OCR 采集场景的 Windows C++ 模块化系统。系统围绕 PLC 作业状态、相机抓拍、OCR 服务识别、结果融合、数据上报和本地存储构建。

整体采用以下架构：

> 主程序动态装载 DLL + ZMQ 消息总线 + Protobuf 统一消息 + PLC 事件驱动工作流

系统不仅执行图像采集，还会把 PLC 作业状态、相机图片、箱号、封志、危险品、残损和车顶号等识别结果整合为一次完整的吊装作业数据。

## 2. 总体架构

```text
                         ┌────────────────────┐
                         │      IARS.exe      │
                         │ 配置、日志、模块加载  │
                         └─────────┬──────────┘
                                   │ LoadLibrary
          ┌────────────────────────┼─────────────────────────┐
          │                        │                         │
          ▼                        ▼                         ▼
 ┌─────────────────┐     ┌──────────────────┐      ┌─────────────────┐
 │ PlcCommDLL      │     │ CameraHK/UNV     │      │ CRSCommDLL      │
 │ 读取岸桥 PLC     │     │ 相机控制/抓拍/PTZ  │       │ OCR/CRS 通信    │
 └────────┬────────┘     └────────┬─────────┘      └────────┬────────┘
          │                       │                         │
          └───────────────────────┼─────────────────────────┘
                                  ▼
                       ┌──────────────────────┐
                       │ZMQ + Protobuf 消息总线│
                       └──────────┬───────────┘
                                  │
             ┌────────────────────┼────────────────────┐
             ▼                    ▼                    ▼
   ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
   │ WorkFlowDLL      │ │ ResultComposeDLL │ │ DataCommDLL      │
   │ 作业状态和流程控制  │ │ 多相机识别结果融合  │ │ 外部通信/可靠转发   │
   └────────┬─────────┘ └────────┬─────────┘ └──────────────────┘
            │                    │
            └──────────┬─────────┘
                       ▼
             ┌──────────────────┐
             │ SaveManagerDLL   │
             │ 图片/视频/空间管理  │
             └──────────────────┘
```

代码中没有传统的中央函数调用链。各模块主要通过 ZMQ 接收、发布 `TransPacket`，并以 Protobuf `Any/type_url` 区分业务消息，从而降低 DLL 之间的直接耦合。

从职责角度，系统可以分为五层：

```text
设备接入层：PLC、CameraHK、CameraUNV、CameraStream
事件与采集层：EventBase、CameraBase
业务编排层：WorkFlowDLL
识别与融合层：CRSCommDLL、ResultComposeDLL
基础设施层：CommonTools、DataCommDLL、SaveManagerDLL
```

## 3. 程序启动过程

主入口位于 `IARS/IARSmain.cpp`，启动过程如下：

1. 设置 DLL 搜索目录并初始化日志。
2. 安装崩溃处理和控制台退出处理。
3. 使用 Windows Mutex 保证只有一个 IARS 实例。
4. 初始化 ZMQ Proxy。
5. 加载 `AppConfig.xml`。
6. 遍历 `<ModuleCfg>` 中配置的 DLL。
7. 使用 `LoadLibrary` 动态加载每个 DLL。
8. 查找模块统一导出接口：
   - `start(configPath)`
   - `stop()`
   - `version()`
9. 调用已启用模块的 `start()`。
10. 记录 DLL 版本、MD5、文件时间和配置文件变更情况。
11. 主线程保持运行，实际业务由各 DLL 内部线程处理。

因此，`IARS.exe` 的主要角色是模块宿主和生命周期管理器，而不是业务逻辑主体。

当前调试配置中启用的核心模块包括：

- `WorkFlowDLL`
- `CameraUNV`
- `SaveManagerDLL`
- `PlcCommDLL`

海康相机、CRS 通信、结果合成和 DataComm 在当前配置中关闭，但代码中仍保留完整实现。

## 4. 核心模块说明

### 4.1 IARS：主控与模块宿主

目录：`IARS/`

主要职责：

- 读取 `AppConfig.xml`。
- 初始化日志与 ZMQ Proxy。
- 动态加载、启动各业务 DLL。
- 单实例控制和崩溃捕获。
- 跟踪 DLL 与配置文件版本。
- 配置变更时备份配置文件。

各模块只需实现统一的 `start/stop/version` 接口，因此可以通过配置启停或替换模块。

### 4.2 WorkFlowDLL：业务流程核心

目录：`WorkFlowDLL/`

这是系统的核心编排模块，主要由以下组件组成。

#### CWorkFlow

负责：

- 初始化 ZMQ 收发端。
- 接收 PLC、OCR 和事件消息。
- 更新 PLC 状态。
- 触发任务管理器。
- 初始化工作流配置和本地数据库。

主要文件：

- `WorkFlowDLL/WorkFlow.h`
- `WorkFlowDLL/WorkFlow.cpp`

#### CTaskManager

负责维护一次吊装作业的完整生命周期：

- 判断当前是否处于有效车道。
- 根据 PLC 状态判断任务开始和结束。
- 生成任务 ID 和保存目录。
- 记录闭锁、开锁时的大车、小车和起升位置。
- 判断装船/卸船、单双箱和吊具类型。
- 收集 OCR 结果及原图、封志、危险品、残损图片。
- 对识别数据进行最终业务整合。
- 保存任务状态，支持重启恢复。
- 发布最终结果。

主要文件：

- `WorkFlowDLL/TaskManager.h`
- `WorkFlowDLL/TaskManager.cpp`

#### StayWorkManager

负责“寄桥作业”这种跨作业状态场景：

- 按大车、小车、起升位置识别寄桥状态。
- 保存未完成或暂存的箱数据。
- 后续作业根据位置重新关联历史数据。
- 处理双吊具取走两个单吊具寄桥箱等特殊情况。

主要文件：

- `WorkFlowDLL/StayWorkManager.h`
- `WorkFlowDLL/StayWorkManager.cpp`

### 4.3 EventBase：PLC 条件与事件模型

目录：`EventBase/`

主要职责：

- 定义事件 `Event` 和条件集合 `EventCond`。
- 保存当前和上一帧 PLC 状态。
- 判断配置条件是否满足。
- 将 PLC 连续数据转换为离散业务事件。

支持的条件包括：

- 大车、小车、起升位置区间。
- 是否跨过指定位置阈值。
- 吊具闭锁/开锁变化。
- 抓箱状态变化。
- 单 20、单 40、双吊具模式。
- CPS 状态、箱位置和距离条件。

其处理模型是：

```text
连续 PLC 数据
    ↓
上一状态 + 当前状态
    ↓
EventCond 条件判断
    ↓
任务开始 / 抓拍 / PTZ 移动 / 作业结束
```

### 4.4 PlcCommDLL：PLC 数据采集适配

目录：`PlcCommDLL/`

主要职责：

- 根据 XML 配置连接 PLC。
- 支持 TCP、UDP、串口等通信形式。
- 周期性读取 PLC 原始数据。
- 解析大车、小车、起升、吊具和闭锁状态。
- 封装为 Protobuf 消息并发布到 ZMQ。

PLC 数据是系统最主要的流程触发源，决定工作流何时开始、何时抓拍以及何时结束。

### 4.5 CameraBase：相机公共抽象层

目录：`CameraBase/`，公共接口位于 `include/CameraBase.h`。

定义厂商无关的相机能力：

- 登录、注销和状态检查。
- 抓取 JPEG。
- 开始和停止录像。
- 设置和读取 PTZ。
- 零位复位。

此外还包含以下通用采集逻辑：

- 接收 PLC 状态。
- 根据事件配置驱动相机。
- 等待 PTZ 到位。
- 抓拍和图片编码。
- 给图片分配全局 `img_label`。
- 将图片及元信息封装为 Protobuf。
- 将图片发送至识别链路。
- 相机异常检查和定时零位复位。

因此，`CameraBase` 同时承担设备抽象和相机事件执行框架的职责。

### 4.6 CameraHK / CameraUNV：相机厂商适配

目录：

- `CameraHK/`
- `CameraUNV/`

二者均继承 `CCameraBase`：

- `CameraHK` 对接海康 `HCNetSDK`。
- `CameraUNV` 对接宇视 `NetDEVSDK`。

厂商模块只实现设备相关操作，事件处理、抓拍消息构造等公共逻辑由 `CameraBase` 提供，属于“抽象基类 + 厂商适配器/工厂”设计。

### 4.7 CameraStream：视频流相机实现

目录：`CameraStream/`

用于视频流或非标准厂商接入。它同样以 DLL 形式运行，但当前主流程主要使用 HK/UNV 设备 SDK 接入。

### 4.8 CRSCommDLL：OCR/CRS 服务通信

目录：`CRSCommDLL/`

主要职责：

- 建立 gRPC 通道。
- 从 ZMQ 获取相机图片和识别请求。
- 向 CRS/OCR 服务发送请求。
- 接收识别结果。
- 将结果重新封装并发布到 ZMQ。
- 管理主、备 OCR 服务状态。

它是本地采集系统与识别服务之间的协议适配层，不负责最终业务结果整合。

### 4.9 ResultComposeDLL：多源识别结果融合

目录：`ResultComposeDLL/`

负责把不同相机、不同 OCR 类型的结果合并为统一任务结果，支持：

- 箱号 `CNR`。
- 封志 `SEALR`。
- 危险品 `IMDGR`。
- 箱门方向 `DR`。
- 箱门开启状态。
- 残损 `DAMAGER`。
- 车顶号 `PMR`。
- 二维码。

箱号融合包含：

- 按图片标签关联相机、分组和事件。
- 多相机投票。
- 结果出现次数和置信度比较。
- 箱号校验。
- 箱主代码相似字符修正。
- ISO 箱型映射。
- 识别不完整时的兜底选择。

目前 `WorkFlowDLL::TaskManager` 也包含一部分最终数据整合逻辑，因此“识别融合”和“业务整合”的边界存在一定重叠。

### 4.10 DataCommDLL：外部消息网关

目录：`DataCommDLL/`

主要职责：

- 在本地 ZMQ Proxy 和外部 Broker 之间转发消息。
- 根据 Protobuf `type_url` 配置消息过滤规则。
- 支持多个 Broker。
- 心跳检测和失败重发。
- 使用 SQLite 保存未确认消息。
- 收到确认后删除历史消息。
- 清理过期数据。

该模块相当于带持久化和重试能力的可靠消息出口。

### 4.11 SaveManagerDLL：文件与磁盘管理

目录：`SaveManagerDLL/`

主要职责：

- 接收图片、视频或任务保存消息。
- 管理数据保存路径。
- 监控磁盘、CPU、内存和 GPU。
- 磁盘使用超过阈值时清理旧数据。
- 控制视频保存和文件生命周期。

该模块将业务逻辑与磁盘容量管理、历史文件清理等运维逻辑解耦。

### 4.12 CommonTools：公共基础设施

目录：`CommonTools/`

包含：

- `ZmqTool` / `ZmqComm`：内部消息通信。
- `LogTool`：日志。
- `TimeTool`：时间处理。
- `UUIDTool`：任务 ID。
- `Utils`：通用工具。
- MD5、RSA、Base64。
- License 校验。
- 公共配置解析。

它是大部分模块都会依赖的公共基础库。

## 5. 核心业务链路

一次典型作业的处理过程如下：

```text
1. PlcCommDLL 读取 PLC
       ↓
2. PLCInfo 通过 ZMQ 发布
       ↓
3. EventBase 判断位置、闭锁、起升等条件
       ↓
4. WorkFlowDLL 创建或推进任务
       ↓
5. CameraBase 根据事件控制 PTZ 和抓拍
       ↓
6. 相机图片携带 img_label 发送给 CRS/OCR
       ↓
7. CRSCommDLL 调用识别服务
       ↓
8. ResultComposeDLL 按 img_label、相机和分组融合结果
       ↓
9. WorkFlowDLL 将识别结果与作业方向、吊具、位置等信息整合
       ↓
10. DataCommDLL 向外部系统可靠上报
       ↓
11. SaveManagerDLL 保存图片、视频并管理磁盘空间
```

核心关联键包括：

- `task_id`：标识一次吊装作业。
- `img_label`：关联一次抓拍及其识别结果。
- `group`：表示双箱或多相机场景中的箱分组。
- `install_name/camera IP`：表示识别结果来源。
- `event_name`：表示触发本次抓拍的 PLC 业务事件。

## 6. 辅助工程

### ConsoleApp

用于测试或验证部分 DLL 接口的命令行程序，不属于生产主流程。

### Rectify

MFC 相机标定/校正工具，用于相机画面或安装参数调试，是独立工具而非运行时核心模块。

### LicenseTool

授权生成或验证工具，与 `CommonTools` 中的 License 能力配套。

### TruckDetect

包含基于 YOLO/TensorRT 的车辆检测、分割及 SmartGate 历史工程。它没有进入当前 `IARS.sln` 的主运行模块列表，更接近实验、旧系统或未来扩展代码，不能视为当前 OCR-IARS 主链路的一部分。

## 7. 架构特点

- DLL 可通过配置启停，现场部署灵活。
- 相机厂商通过公共基类隔离，扩展新设备的成本较低。
- ZMQ + Protobuf 降低模块间的直接依赖。
- PLC 连续信号与离散业务事件进行了分层。
- DataComm 具备 SQLite 持久化和失败重发能力。
- 对现场运行考虑较完整，包括单实例、崩溃捕获、配置备份和磁盘监控。

## 8. 潜在问题与改进方向

- 主程序使用永久循环维持运行，没有完整的正常退出和统一卸载流程。
- DLL 生命周期接口通过字符串动态查找，缺少编译期约束。
- ZMQ 消息主题主要依赖 Protobuf `type_url`，消息契约较为分散。
- `CameraBase` 同时承担设备抽象、事件执行、图片处理和消息发送，职责偏重。
- `TaskManager` 数据成员和业务分支较多，是明显的复杂度中心。
- `ResultComposeDLL` 与 `TaskManager::DataIntegration` 都包含结果整合逻辑，边界需要进一步明确。
- `AppConfig.xml` 同时包含模块、PLC、用户、监控和融合映射等多类配置，配置域较集中。
- 仓库同时包含生产模块、调试工具和历史 TruckDetect 工程，目录边界不够清晰。
- 部分源文件存在编码混杂和注释、日志乱码问题，影响维护体验。

## 9. 推荐阅读顺序

理解系统时，建议按照以下代码路径阅读：

```text
IARS/IARSmain.cpp
    ↓
WorkFlowDLL/WorkFlow.cpp：CWorkFlow::RecvCallFunc
    ↓
WorkFlowDLL/TaskManager.cpp：CTaskManager::TaskTrigger
    ↓
CameraBase/CameraBase.cpp：CCameraBase::Action / DoCapture
    ↓
ResultComposeDLL/ResultCompose.cpp：识别结果融合
    ↓
WorkFlowDLL/TaskManager.cpp：DataIntegration / SendResult
```

该路径覆盖模块加载、PLC 消息接收、任务状态机、相机抓拍、识别融合以及最终结果发送，是掌握 OCR-IARS 主流程的最短路径。
