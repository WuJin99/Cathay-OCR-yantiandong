#pragma once
#include "PLCInfo.h"

class PLCInfoState
{
public:
	PLCInfoState();
	~PLCInfoState();
public:
	// 更新PLC状态
	void Update(const PLCInfo& plcInfo);
	// 着箱信号消失
	bool isUnCatchOccur() const;
	// 是否发生着箱
	bool isCatchOccur() const;
	// 是否发生开锁
	bool isUnLockOccur() const;
	// 是否发生闭锁
	bool isLockOccur() const;
	// 吊具类型发生变化
	bool isSpreaderMode() const;
	bool isTrollyPassValue(int value) const;
	bool isTrollyPassValueUp(int value) const;
	bool isTrollyPassValueDown(int value) const;
	// 下降
	bool isHoistPassValueDown(int value) const;
	// 上升
	bool isHoistPassValueUp(int value) const;
	bool isHoistPassValue(int value) const;
	//吊具到目标箱顶距离经过
	bool isHoistToContPassValue(int value) const;
	//吊具到目标箱顶距离减小经过
	bool IsHoistToContPassValueDown(int value) const;
	//吊具到目标箱顶距离变大经过
	bool IsHoistToContPassValueUp(int value) const;
	//空吊具下放
	bool IsEmptyHoistDownOccur() const;
	//空吊具上升
	bool IsEmptyHoistUpOccur() const;
	//带箱起升
	bool IsContHoistUpOccur() const;
	//吊具上升发生
	bool IsHoistUpOccur() const;
	//吊具下降发生
	bool IsHoistDownOccur() const;
	//吊具是否是上升状态
	bool IsHoistUpStatus() const;
	//移箱
	bool IsMoveContOccur() const;
	//带箱下降
	bool IsContHoistDownOccur() const;
	// 返回当前PLC信息
	const PLCInfo& GetCurPLCInfo() const;
	const PLCInfo& GetLastPLC() const;
	// 返回当前识别到的动作
	std::string GetMovement() const;
	std::string GetMovement(int trollyValue, int hoistValue) const;
	//返回吊具连续上升的值，不是上升状态此值为0
	int GetHoistAboveValue() const;
	//吊具上升某值发生
	bool IsHoistAbovePassValueOccur(int value) const;
	//车号结果从无到有
	bool IsPmnResultHaveOccur() const;
	//箱号结果从无到有
	bool IsCmnResultHaveOccur() const;
	//集卡到位发生
	bool IsCpsStatusFinishOccur() const;
	//集卡离开发生
	bool IsCpsStatusLeaveOccur() const;
	//对位距离减小经过某值
	bool IsCpsDistanceDownPassValue(int value) const;

	//大车是否移动发生
	bool IsGantryMoveOccur() const;
	//大车是否移动中
	bool IsGantryMoving() const;
	//大车是否向头间（小间位）移动发生
	bool IsGantryHeadMoveOccur() const;
	//大车是否向尾间（大间位）移动发生
	bool IsGantryTailMoveOccur() const;

//2026.2 对应三一增加车道下是否有车接口，增加判断事件
	//1车道是否有集卡车
	bool IsTruckInLane1()const;
	//2车道是否有集卡车
	bool IsTruckInLane2()const;
	//1车道集卡离开是否发生
	bool IsLane1LeaveOccur() const;
	//2车道集卡离开是否发生
	bool IsLane2LeaveOccur() const;

	//大车是否向左移动大于某值发生
	bool IsGantryLeftMovePassValueOccur(const int value);
	//大车是否向右移动大于某值发生
	bool IsGantryRightMovePassValueOccur(const int value);

	//获取大车持续left移动的距离
	int GetGantryLeftMoveValue() const;
	//获取大车持续right移动的距离
	int GetGantryRightMoveValue() const;

	//获取大车总共移动的距离 程序启动时为初始值
	int GetTotalGantryMoveValue() const;

	//大车每次移动超过某值
	bool IsGantryEveryPassValueOccur(int value);
	//PLC中是否开始发送结果
	bool IsStartSendResultToPlc() const;
	bool IsStopSendResultToPlc() const;

	//开始处理押后集装箱
	bool DealWithBackContainerOccur()const;
	unsigned short TaskStatusChangeOccur()const;
	//Taskid
	bool TaskIdChangeOccur()const;
	//Taskid
	bool CnPosBCountPass(int v)const;

	bool PlcPacketsCounterForTaskPass(int v)const;


	int cnPosBcountCur;		// g_CmnResultState的 上一次值，用来判断箱号从无到有的发生
	int cnPosBcountOld;
	int plcPacketsCounterForTask_old;  // 
	int plcPacketsCounterForTask_cur;  // 计数器，开始一个新任务时清空该计数器，然后每获取一包PLC数据则更新该值
	
private:
	bool firstFlag = true;  //启动程序后的第一次赋值标志
	int gantryStartValue;	//程序启动时大车的位置 大车的起始位置
	int gantryLeftMoveValue;		//大车持续左移动距离；
	int gantryRightMoveValue;		//大车持续右移动距离；
	int trollyMin;      	// 根据小车编码器的范围值触发
	int trollyMax;
	int hoistMin;       	// 根据起升编码器范围值触发
	int hoistMax;
	int gantryMin;       	// 根据大车编码器范围值触发
	int gantryMax;

	int hoistAbove;			//吊具上升状态时 连续上升的值
	int hoistAboveOld;		//吊具上升状态时 连续上升的值

	int pmmResultStateCur;		// g_PmnResultState的 最新值，用来判断车号从无到有的发生
	int pmmResultStateOld;		// g_PmnResultState的 上一次值，用来判断车号从无到有的发生
	int cmmResultStateCur;		// g_CmnResultState的 最新值，用来判断箱号从无到有的发生
	int cmmResultStateOld;		// g_CmnResultState的 上一次值，用来判断箱号从无到有的发生

	mutable std::mutex g_mtxGantry;
	const unsigned int MAX_FRAMES = 10;   //队列保存的最大帧数 10
	std::queue<int> g_queGantry;//大车编码器值队列，用来保存最新10帧的大车编码器值
	PLCInfo m_oldPLC;
	PLCInfo m_curPLC;


};

template<typename OStream>
OStream& operator << (OStream& out, const PLCInfoState& plcInfoState)
{
	if (!g_AppConfig.userCfg.bPlcState)
	{
		return out;
	}
	auto& curPlc = plcInfoState.GetCurPLCInfo();
	auto& oldPlc = plcInfoState.GetLastPLC();

	out << "\n";
	out << std::setw(14) << "消息编号: " << oldPlc.usMsgNum << "->" << curPlc.usMsgNum;
	out << "\n";
	out << std::setw(14) << "大车编码器: " << oldPlc.gantryValue << "->" << curPlc.gantryValue;
	out << "\n";
	out << std::setw(14) << "小车编码器: " << oldPlc.trollyValue << "->" << curPlc.trollyValue;
	out << "\n";
	out << std::setw(14) << "起升编码器: " << oldPlc.hoistValue << "->" << curPlc.hoistValue;
	out << "\n";
	out << std::setw(14) << "小车状态: " << oldPlc.GetTrollyStatus();
	out << "(" << oldPlc.trollyStatus << ")->" << curPlc.GetTrollyStatus() << "(" << curPlc.trollyStatus << ")";
	out << "\n";
	out << std::setw(14) << "起升状态: " << oldPlc.GetHoistStatus();
	out << "(" << oldPlc.hoistStatus << ")->" << curPlc.GetHoistStatus() << "(" << curPlc.hoistStatus << ")";
	out << "\n";
	out << std::setw(14) << "开闭锁状态: " << oldPlc.GetLockStatus();
	out << "(" << oldPlc.lockStatus << ")->" << curPlc.GetLockStatus() << "(" << curPlc.lockStatus << ")";
	out << "\n";
	out << std::setw(14) << "着箱状态: " << oldPlc.GetCatchStatus();
	out << "(" << oldPlc.catchStatus << ")->" << curPlc.GetCatchStatus() << "(" << curPlc.catchStatus << ")";
	out << "\n";
	out << std::setw(14) << "吊具宽度: " << oldPlc.GetSpreadMode();
	out << "(" << oldPlc.spreaderMode << ")->" << curPlc.GetSpreadMode() << "(" << curPlc.spreaderMode << ")";
	out << "\n";
	//wxm 2020.12.13
	out << std::setw(14) << "海侧车道: " << oldPlc.workLine1 << "->" << curPlc.workLine1;
	out << "\n";
	out << std::setw(14) << "陆侧车道: " << oldPlc.workLine2 << "->" << curPlc.workLine2;
	out << "\n";
	//wxm 2020.11.25
	//out << std::setw(14) << "吊具形态: " << oldPlc.GetSpreadShape();
	//out << "(" << oldPlc.spreaderShape << ")->" << curPlc.GetSpreadShape() << "(" << curPlc.spreaderShape << ")";
	//out << "\n";
	out << std::setw(14) << "海侧CPS对位距离: " << oldPlc.cpsDistance << "->" << curPlc.cpsDistance;
	out << "\n";
	//wxm 2020.11.25 增加以下4个值的输出
	out << std::setw(14) << "海侧CPS对位状态: " << oldPlc.cpsStatus << "->" << curPlc.cpsStatus;
	out << "\n";
	out << std::setw(14) << "陆侧CPS对位距离: " << oldPlc.cpsDistanceLand << "->" << curPlc.cpsDistanceLand;
	out << "\n";
	out << std::setw(14) << "陆侧CPS对位状态: " << oldPlc.cpsStatusLand << "->" << curPlc.cpsStatusLand;
	out << "\n";
	out << std::setw(14) << "CPS带箱状态: " << oldPlc.cpsStatusCase << "->" << curPlc.cpsStatusCase;
	out << "\n";
	out << std::setw(14) << "CPS压箱位置: " << oldPlc.cpsCnPos << "->" << curPlc.cpsCnPos;
	out << "\n";
	out << std::setw(14) << "CPS车头方向: " << oldPlc.GetCpsDir();
	out << "(" << oldPlc.cpsDirection << ")->" << curPlc.GetCpsDir() << "(" << curPlc.cpsDirection << ")";
	out << "\n";
	//out << std::setw(14) << "集卡车头方向: " << oldPlc.GetTruckDir();
	//out << "(" << oldPlc.truckDirection << ")->" << curPlc.GetTruckDir() << "(" << curPlc.truckDirection << ")";
	//out << "\n";

	out << std::setw(14) << "动作: " << plcInfoState.GetMovement();
	return out;
}
