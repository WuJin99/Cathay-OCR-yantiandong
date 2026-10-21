#ifndef CAMERACONFIG_H
#define CAMERACONFIG_H

#include "Utils.h"

#include "PLCInfo.h"
#include "PLCInfoState.h"
#include <mutex>

class Config
{
public:
    // 该配置节点的父节点名
    std::string groupName = "";

    // 该配置在xml文件中的节点名
    std::string nodeName = "cfgNode";

    // 从配置文件中加载配置
    virtual bool load(const std::string xmlPath);

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode){ return true; }

    Config(std::string nn, std::string gn)
        : nodeName(nn)
        , groupName(gn)
    {}
};

//摄像头识别类型及参数配置
struct RecogParam
{
	int truckType;//集卡类型
	//标签名称
	std::string name;
	//识别类型
	ocrs::pb::GRpc_RecogParam::G_RType rtype;
	// 识别区域
	cv::Rect roi;
	std::string roi_str;
	// 需要反转（转变为灰度图）
	bool reverse;
	// 旋转角度,正数=逆时针
	int rotate;
	// 矫正
	std::string rectify;
};

struct PTZ
{
	// PTZ 参数值
	int P, T, Z;
	////旋转方式，0绕x轴旋转，1绕y轴旋转，-1绕原点旋转
	//std::string rotateType;

	bool operator== (const PTZ& p) const { return P == p.P && T == p.T && Z == p.Z; }
	bool operator!= (const PTZ& p) const { return P != p.P || T != p.T || Z != p.Z; }
};

/*-------------------事件-----------------------*/
// 事件触发条件
class EventCond :public Config
{
public:
    std::map<std::string, int> facets;
	bool m_GetCnResult;
	bool m_GetPmnResult;
	std::string cond_name;
    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);
    EventCond(std::string nn = "", std::string gn = "Event")
        :Config(nn, gn)
    { }
    // 判断条件是否满足
    bool Satisfied(const PLCInfoState& plcInfoState, std::string& cond_info, bool getCnResult, bool getPmnResult) const;
};

// 车道配置
class LaneConfig :public Config
{
public:
    struct Range
    {
        int minValue;
        int maxValue;
		int laneNum; //车道号
    };
    std::map<std::string, Range> m;

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);
    LaneConfig(std::string nn = "", std::string gn = "Lanes")
        :Config(nn, gn)
    {}
};

// 车道配置
class PositionConfig :public Config
{
public:
	
	std::map<std::string, int> m;

	// 将xml节点转换成配置
	virtual bool deserialize(std::string node_name, const ptree pNode);
	PositionConfig(std::string nn = "", std::string gn = "Position")
		:Config(nn, gn)
	{}
};

// 事件
class Event :public Config
{
public:
    // 事件类型
	std::string evtType;
	// 事件名称
	std::string evtName;
	
	//事件发生时是否发送结果
	bool sendResult = false;

	// 跳转完成后自动抓拍关联事件
	std::string autoCap;

	//相机跳转PTZ
	PTZ ptz;

	//默认为空，用于根据ECS发送的JobTrigger中truck_dir、cont_pos和吊具类型进行匹配，只选用对应的相机进行工作
	std::string spreader;
	std::string truck_dir;
	std::string cont_pos;

	//相机抓拍后识别参数
	std::vector<RecogParam> arrRecogParam;
	//联动事件名称
	std::vector<std::string> arrStepEvent;
    
    // 触发事件的必要条件: 该条件中的所有条件都满足是才会触发
    EventCond necessaryCond;
    // 触发事件的充分条件: 当必要条件满足时该条件中的一个满足就会触发
    std::map<std::string, EventCond> sufficientConds;

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);

    // 是否触发
    bool Triggered(const PLCInfoState& plcInfoState, std::string event_name, bool getCnResult, bool getPmnResult) const;

    Event(std::string nn = "", std::string gn = "Event")
        :Config(nn, gn)
    {}
};


// 事件
class TaskEventConfig :public Config
{
public:
    // 事件
    std::map<std::string, Event> eventList;

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);

    TaskEventConfig(std::string nn = "", std::string gn = "Task")
        :Config(nn, gn)
    {}
};

// 事件配置参数
class EventsConfig :public Config
{
public:
    // 事件
    std::map<std::string, Event> eventList;

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);
    EventsConfig(std::string nn = "", std::string gn = "Events")
        :Config(nn, gn)
    {}
};
/*-------------------事件-----------------------*/

class IPCConfig : public Config
{
public:
	bool cnFirst;//箱号优先使用
    std::string camName = "IPC";     // 用户名

    std::string ip;                     // 相机IP地址

    std::string userName = "admin";     // 用户名

    std::string passwd = "asdf1234";    // 密码

    int nPort = 8000;                   // SDK端口

	int lane;                   // 所属车道

    bool usable = true;                 // 是否使用该设备

    // 将配置转换成xml节点
    virtual bool deserialize(std::string node_name, const ptree pNode);

    IPCConfig(std::string nn = "IPC", std::string gn = "IPC")
        :Config(nn, gn)
    {}
};

class HKGunConfig : public IPCConfig
{
public:
    EventsConfig events;

    // 将xml节点转换成配置
    virtual bool deserialize(std::string node_name, const ptree pNode);

    HKGunConfig(std::string nn = "", std::string gn = "HK")
        :IPCConfig(nn, gn)
    { }
};

class HKPlateConfig : public IPCConfig
{
public:
	HKPlateConfig(std::string nn = "", std::string gn = "HKPlate")
		:IPCConfig(nn, gn)
	{ }
};

class DHCamConfig : public IPCConfig
{
public:
	EventsConfig events;

	// 将xml节点转换成配置
	virtual bool deserialize(std::string node_name, const ptree pNode);

	DHCamConfig(std::string nn = "", std::string gn = "DH")
		:IPCConfig(nn, gn)
	{ }
};
class AxisF44Config : public Config
{
public:
	bool cnFirst;//箱号优先使用
	std::string camName = "Axis";     // 用户名

	std::string ip;                     // 相机IP地址

	std::string userName = "root";     // 用户名

	std::string passwd = "asdf1234";    // 密码
	int lane;//所在车道
	int nPort = 8000;                   // SDK端口

	bool usable = true;                 // 是否使用该设备

	std::string channel;                 //F44共4个相机插口，表示通道，取值1~4

	EventsConfig events;
	// 将配置转换成xml节点
	virtual bool deserialize(std::string node_name, const ptree pNode);

	AxisF44Config(std::string nn = "", std::string gn = "AxisF44")
		:Config(nn, gn)
	{}
};

class ICEConfig : public Config
{
public:
	bool usable;                        // 是否使用该设备
	int lane;							//所属哪个车道
	std::string name;                   // 相机标识名

	std::string ip;                     // 相机IP地址


	// 将配置转换成xml节点
	virtual bool deserialize(std::string node_name, const ptree pNode);

	ICEConfig(std::string nn = "", std::string gn = "ICE")
		:Config(nn, gn)
	{}
};

//全局车道配置参数，是每个CameraConfig中车道的汇总
extern LaneConfig g_LaneCfg;

extern PositionConfig g_PositionCfg;
extern std::mutex g_PositionCfgMutex;
#endif