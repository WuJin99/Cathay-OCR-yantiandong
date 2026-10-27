#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <string>
#include <set>
#include "Utils.h"

// 加载xml配置文件
void LoadAppConfigXML(const std::string xmlPath);

struct FTPConfig              // FTP配置
{
	bool enable = 0;            // 启用
	std::string ftpHost; // FTP地址
	std::string ftpPort;               // FTP端口
	std::string ftpUser; // FTP用户名
	std::string ftpPwd; //  FTP密码
	int reSaveCounts = 20; //  文件上传失败时，最多尝试上传的次数，大于此值时就不再尝试上传
};

struct CrsConfig              // CRS配置
{
	std::string crs_name;
	std::string ipaddr;
	std::string port;
};

struct BrokerConfig // 消息代理通信配置参数
{
    std::string IP;
	int xpub_port;
	int sub_port;
	int resend_count; //消息最大重复发送的次数
};

struct MonitorConfig // 网口通信配置参数
{
	int CPUCfg;
	int GPUCfg;
	int diskCfg;//磁盘剩余空间百分比
	int memCfg;
};

struct UserConfig
{
	std::string imgRootPath;        // 图片映射根路径
	bool pmnSimilarMatch;				//车号模糊匹配
    std::string quayNo = "未知";        // 岸桥编号
    std::string IARSName = "未知";      // 岸桥编号
    bool bplcshow = false;              // 是否显示PLC
	int sendInfoShow = 0;              //是否显示发送的消息。0都不显示，1追加显示OCRData 2.追加显示EventInfo;3.追加显示StateInfo;4.追加显示HeartBeat
    std::string localIp = "127.0.0.1";       // 本机IP
	std::string savePath = "D://";       // 存储根目录
    bool bptzDebug = false;              // 是否显示PLC
	bool bPlcState;                      // 满足触发条件时显示PLC状态变化
	int stateInfoInterval;                 // 发送相机、PLC、工控机状态信息的间隔秒数
	int heartBeatInterval;                 // 发送心跳信息的间隔秒数
	int needReplyResend;					//待回复的消息，超时秒数，超时重发
	int heartBeatTimeout;                 // 心跳超时秒数
	int wait_ptz_sec;					//set后多少秒进行拍照拍照
	int gantryChangeBayValue;
	bool sdk_log;//是否开启海康和车牌相机日志
	int ecs_ms;//接收到JobTrigger后延时发送结果到ECS
	bool only_pmn;//轻轨，只有车号功能
	float door_open_conf;//箱门打开结果有效置信度，高于该值才进行处理
	int door_open_count;//箱门打开结果累计计数，高于该值时才输出到PLC，防止误报
	int reset_dooropen_by_hoist_value;//空吊具上升经过该值时复位箱门打开结果
	int sendTruckInterval; //车号结果发送到plc的最小间隔
	int sendTruckTimeOut;//请求车号结果之后 超时时间 ms
	int hoistToContUpVal;//吊具到箱顶距离向上经过该值时清空箱号结果与箱号图片，单位cm，默认200（与CAP_TOP下降触发一致），023.4.4版本为300
	int pmn_ganty; //动大车时车牌识别位置差，默认400
	int CRS_GRPC_WaitTime;//GRPS超时时间

	// 车顶号：入池记样本；PIIS 拉号时按请求瞬间 gantry 窗口聚合 conf/nCount（不要求大车停稳）
	bool pmnWindowEnable;              // 1=启用入库门控（默认仅 hasTruck）；0=不门控，有效结果都入池
	int pmnWindowGantryDelta;          // 拉号窗口：|样本gantry - PIIS请求时gantry| 容差，车顶号/车牌发送共用，默认1300≈13m
	int pmnNoTruckClearFrames;         // hasTruck 1→0 连续无车帧数达此值才清车顶号池与车牌，默认3；0=不去抖(1帧即清)
	int pmnPlateEarlyHoldMs;           // 车牌在 hasTruck=0 时暂存时长(ms)，有车后再入主池；默认2000；0=不暂存(无车直接丢)
	int pmnRecentStreakK;              // 拉号：窗内按入池顺序最近连续同号≥K则选该号；默认3；≤0关闭
	int pmnRecentFallbackM;            // 拉号：未达 streak 时仅用窗内最近 M 条做 nCount/conf；默认10；≤0用窗内全部
};

class TaskManager;
struct LaneSide
{
	std::string name;
	std::string lane_id;
	std::string cfg;
	boost::shared_ptr<TaskManager> pTaskMgr;
};

struct SubConfig
{
	std::string ipaddr;
	int port;
	void *pObj;//CZmqComm*
};

struct PlateMatch
{
	std::string plate_num;//车牌号
	std::string truck_num;//车顶号
	float truck_num_conf;
	std::string type;//I/O 内外集卡
};
struct PLCHttpConfig      // PLC模块 网口配置
{

	std::string remoteIP;
	int remotePort;
	int serverPort;
	std::string plcPath;
	std::string localHostIP;
	std::string doorDirPath;
	std::string doorOpenPath;
	std::string cnDataPath;
	std::string pmnDataPath;
	int waitTime;//请求间隔时间 单位ms
};
struct PLCConfig              // PLC 通信配置参数
{
	int commType;               // PLC通信方式
	int slaveAddr;              // 从站地址slaveAddr
	int readAddr;
	int writeAddr;
	int rrTimeout;              // 读反馈超时时间毫秒rrTimeout
	int rrWaitTime;             // 两次读数据的间隔时间rrWaitTime


	PLCHttpConfig plcHttpCfg;       // PLC 网口通信配置参数
};
struct AppConfig
{
    UserConfig userCfg;             //用户配置
	BrokerConfig brokerCfg;			//消息代理配置
	FTPConfig ftpCfg;               // FTP上传配置
	MonitorConfig monitorCfg;		//工控机资源阈值配置
	PLCConfig plcCfg;

	std::map<std::string, std::string> mapOwnerNum;
	std::map<std::string, std::string> mapIsoNum;
	std::vector<LaneSide> arrLaneSide;
	std::vector<CrsConfig> arrCrsCfg;
	std::vector<std::string> arrInvalidPlate;//无效车牌结果，车牌相机会将内集卡车牌号误识别，通过这个字段进行排除
	std::vector<SubConfig> arrSubCfg;
	std::vector<PlateMatch> arrPlateMatch;
	//std::vector<string>m_PmnComp;//保存车顶号前n位对应的字符集
};

extern AppConfig g_AppConfig;
#endif
