
#include "PLCInfo.h"

std::map<std::string, PLCInfo*> g_plcSubscriber;

boost::mutex g_PlcUpdateMutex; // 全局互斥锁.

boost::condition_variable g_PlcUpdateCond; // 全局条件变量.

OCRToPlc g_OCRToPlc;
std::mutex g_mtxOCRToPlc;
std::atomic<int> g_nLockTrallyValue = 0;
std::atomic<int> g_nLockHoistValue = 0;
//std::atomic<int> g_nLockHoistValue1 = 0;//用于单起升方向移动时箱门打开事件
//
//boost::mutex g_PlcUpdateMutexOne; // 全局互斥锁.
//
//boost::condition_variable g_PlcUpdateCondOne; // 全局条件变量.


void SubscribePLC(std::string camIP, PLCInfo &plcInfo)
{
    if (g_plcSubscriber.find(camIP) == g_plcSubscriber.end())
    {
        g_plcSubscriber[camIP] = &plcInfo;
        console->info("注册PLC：{} 成功 size:{}", camIP, g_plcSubscriber.size());
    }
    else
    {
        console->debug("注册PLC：{} 失败 size:{}", camIP, g_plcSubscriber.size());
    }   
}

void UnSubscribePLC(std::string camIP)
{
    if (g_plcSubscriber.find(camIP) != g_plcSubscriber.end())
    {
        g_plcSubscriber.erase(camIP);
        console->info("反注册PLC：{} 成功 size:{}", camIP, g_plcSubscriber.size());
    }
    else
    {
        console->debug("反注册PLC：{} 失败 size:{}", camIP, g_plcSubscriber.size());
    }
}

unsigned short PLCInfo::ParseShort(const unsigned char *plcData, int& offset)
{
	unsigned short usValue = plcData[offset] * (1 << 8) + plcData[offset + 1];
	offset += 2;
	return usValue;
}

int PLCInfo::ParseInt(const unsigned char *plcData, int& offset)
{
	int nValue = plcData[offset] << 24 & 0xff000000;
	nValue |= plcData[offset+1] << 16 & 0xff0000;
	nValue |= plcData[offset+2] << 8 & 0xff00;
	nValue |= plcData[offset+3] & 0xff;
	offset += 4;
	return nValue;
}
void PLCInfo::ParseMsg(const ptree& plc)
{
	uuid = plc.get<string>("uuid");
	timeStamp = plc.get<string>("timestamp");
	g_testTaskIdMtx.lock();
	if (g_testTaskId .empty())
	{

		taskId = plc.get<string>("taskId");
	}
	else
	{
		taskId = g_testTaskId;
	}
	g_testTaskIdMtx.unlock();
	craneId = plc.get<string>("craneId");
	truckNumStr = plc.get<string>("truckNo");
	containerNo1 = plc.get<string>("containerNo1");
	containerNo2 = plc.get<string>("containerNo2");


	taskState = plc.get<float>("taskState");

	hoistValue = plc.get<float>("positionHoist")* 100;
	trollyValue = plc.get<float>("positionTrolley") * 100;
	gantryValue = plc.get<float>("positionGantry") * 100;

	gantrySpeed = plc.get<float>("speedGantry") * 100;
	trollySpeed = plc.get<float>("speedTrolley") * 100;
	hoistSpeed = plc.get<float>("speedHoist") * 100;

	lockStatus = plc.get<short>("statusLock");
	
	if (g_testCatch > 0)
	{
		catchStatus = g_testCatch;
	}
	else
	{
		catchStatus = plc.get<short>("statusLand");
	}
	spreaderMode = plc.get<short>("spreaderSize");
	if (g_testLane > 0 )
	{
		workLine = g_testLane;
	}
	else
	{
		workLine = plc.get<short>("workLane");
	}
	truckType = plc.get<short>("truckType");

	//三一新定义接口，hasTruck[],下标对应车道
	const ptree& arr = plc.get_child("hasTruck");

	// hasTruck[0] 无效，忽略
	int idx = 0;
	for (const auto& item : arr) {
		if (idx >= 3) break;  // 只取前3个
		hasTruck[idx] = item.second.get_value<short>(0);
		++idx;
	}
	if (idx < 3) {
		for (int i = idx; i < 3; ++i) hasTruck[i] = 0;
	}
	cpsDistance = plc.get<float>("truckAlignmentDistance");
	hoistToCont = plc.get<float>("spreaderFromContainerHeight") * 100;

	cpsStatus = plc.get<short>("truckAlignmentStatus");
	workType = plc.get<short>("workType");
	cpsCnPos = plc.get<short>("containerOnTruck");
}
