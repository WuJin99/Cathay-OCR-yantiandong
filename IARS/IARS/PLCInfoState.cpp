#include "PLCInfoState.h"
// #include "CameraConfig.h"

PLCInfoState::PLCInfoState()
	: trollyMin(0)
	, trollyMax(0)
	, hoistMin(0)
	, hoistMax(0)
	, gantryMin(0)
	, gantryMax(0)
	, hoistAbove(0)
	, hoistAboveOld(0)
	, pmmResultStateCur(0)
	, pmmResultStateOld(0)
	, cmmResultStateCur(0)
	, cmmResultStateOld(0)
	, gantryLeftMoveValue (0)
	, gantryRightMoveValue (0)
{


}

PLCInfoState::~PLCInfoState()
{

}
// 更新PLC状态
void PLCInfoState::Update(const PLCInfo& plcInfo)
{
	//临时变量，用来去除无效的锁状态
	int lockTemp;
	if (!firstFlag)
	{

		lockTemp = m_curPLC.lockStatus;
		m_oldPLC = m_curPLC;
		cnPosBcountOld = cnPosBcountCur;
		//有无 车号
		pmmResultStateOld = pmmResultStateCur;

		//有无 车号
		cmmResultStateOld = cmmResultStateCur;
		plcPacketsCounterForTask_old = plcPacketsCounterForTask_cur;
	}
	//第一次运行
	else
	{
		cnPosBcountCur = 0;
		cnPosBcountOld = 0;
		//保存第一次的锁状态
		lockTemp = plcInfo.lockStatus;

		m_oldPLC = plcInfo;
		//有无 车号
		pmmResultStateOld = 0;

		//有无 车号
		cmmResultStateOld = 0;
		//大车的起始位置
		gantryStartValue = plcInfo.gantryValue;
		plcPacketsCounterForTask_old = 0;
		plcPacketsCounterForTask_cur = 0;
	}
	//当锁的状态不是无效时才赋值
	if (plcInfo.lockStatus != PLCInfo::LOCK_INVALID)
	{
		lockTemp = plcInfo.lockStatus;
	}

	m_curPLC = plcInfo;
	if (plcInfo.cpsCnPos == PLCInfo::CN_POSITION::CN_POS_B)
	{
		cnPosBcountCur += 1;
	}
	else
	{
		cnPosBcountCur = 0;
	}

	if (TaskIdChangeOccur())
	{
		plcPacketsCounterForTask_old = 0;
		plcPacketsCounterForTask_cur = 0;
	}
	else if (plcInfo.cpsStatus == 1)//对位完成计数
	{
		plcPacketsCounterForTask_cur++;
	}

	//如果第一次赋值是第一个plcinfo lockstatus的值，其他当plcinfo 锁状态无效时赋值上次锁的状态
	m_curPLC.lockStatus = lockTemp;

	//pmmResultStateCur = g_TaskInfo.pmn_result();
	//cmmResultStateCur = g_TaskInfo.cmn_result();


	firstFlag = false;


	//连续上升高度
	hoistAboveOld = hoistAbove;
	if (IsHoistUpStatus())
	{
		hoistAbove += abs(m_curPLC.hoistValue - m_oldPLC.hoistValue);
	}
	else
	{
		hoistAbove = 0;
	}
	if (g_mtxGantry.try_lock())
	{
		std::lock_guard<std::mutex> lock(g_mtxGantry, std::adopt_lock);
		g_queGantry.push(m_curPLC.gantryValue);//入队最新帧数据，并出队多余帧数据
		if (g_queGantry.size() > MAX_FRAMES)
		{
			g_queGantry.pop();
		}
	}
}
// 着箱信号消失
bool PLCInfoState::isUnCatchOccur() const
{
	if (m_oldPLC.catchStatus == PLCInfo::CATCHING
		&& m_curPLC.catchStatus != PLCInfo::CATCHING)
	{
		return true;
	}
	return false;
}
// 是否发生着箱
bool PLCInfoState::isCatchOccur() const
{
	if (m_oldPLC.catchStatus != PLCInfo::CATCHING
		&& m_curPLC.catchStatus == PLCInfo::CATCHING)
	{
		return true;
	}
	return false;
}
// 是否发生开锁
bool PLCInfoState::isUnLockOccur() const
{
	if (m_oldPLC.lockStatus != PLCInfo::UNLOCKED
		&& m_curPLC.lockStatus == PLCInfo::UNLOCKED)
	{
		return true;
	}
	return false;
}

// 是否发生闭锁
bool PLCInfoState::isLockOccur() const
{
	if (m_oldPLC.lockStatus == PLCInfo::UNLOCKED
		&& m_curPLC.lockStatus == PLCInfo::LOCKED)
	{
		return true;
	}
	return false;
}
// 吊具类型发生变化
bool PLCInfoState::isSpreaderMode() const
{
	if (m_curPLC.spreaderMode != m_oldPLC.spreaderMode)
	{
		return true;
	}
	return false;
}

bool PLCInfoState::isTrollyPassValue(int value) const
{
	if (isTrollyPassValueUp( value)
		|| isTrollyPassValueDown( value))
	{
		return true;
	}
	return false;
}
bool PLCInfoState::isTrollyPassValueUp(int value) const
{
	if (m_oldPLC.trollyValue <= value && m_curPLC.trollyValue > value)
	{
		return true;
	}
	return false;
}
bool PLCInfoState::isTrollyPassValueDown(int value) const
{
	if (m_oldPLC.trollyValue >= value && m_curPLC.trollyValue < value)
	{
		return true;
	}
	return false;
}
// 下降
bool PLCInfoState::isHoistPassValueDown(int value) const
{
	if (m_oldPLC.hoistValue >= value && m_curPLC.hoistValue < value)
	{
		return true;
	}
	return false;
}
// 上升
bool PLCInfoState::isHoistPassValueUp(int value) const
{
	if (m_oldPLC.hoistValue <= value && m_curPLC.hoistValue > value)
	{
		return true;
	}
	return false;
}
bool PLCInfoState::isHoistPassValue(int value) const
{
	if ((m_oldPLC.hoistValue <= value && m_curPLC.hoistValue > value)
		|| (m_oldPLC.hoistValue >= value && m_curPLC.hoistValue < value))
	{
		return true;
	}
	return false;
}

bool PLCInfoState::isHoistToContPassValue(int value) const
{
	if ((m_oldPLC.hoistToCont <= value && m_curPLC.hoistToCont > value)
		|| (m_oldPLC.hoistToCont >= value && m_curPLC.hoistToCont < value))
	{
		return true;
	}
	return false;
}

bool PLCInfoState::IsHoistToContPassValueDown(int value) const
{
	if (m_oldPLC.hoistToCont > value && m_curPLC.hoistToCont <= value && m_curPLC.hoistToCont > 0)
	{
		if (m_oldPLC.hoistToCont - m_curPLC.hoistToCont < value)//屏蔽跳变的值
			return true;
	}
	return false;
}

bool PLCInfoState::IsHoistToContPassValueUp(int value) const
{
	if (m_oldPLC.hoistToCont < value && m_curPLC.hoistToCont >= value && m_curPLC.hoistToCont > 0)
	{
		if (m_curPLC.hoistToCont - m_oldPLC.hoistToCont < value)//屏蔽跳变的值
			return true;
	}
	return false;
}

bool PLCInfoState::IsEmptyHoistDownOccur() const
{
	if (m_oldPLC.lockStatus == PLCInfo::UNLOCKED
		&& m_oldPLC.hoistStatus != PLCInfo::HOIST_DOWN
		&& m_curPLC.lockStatus == PLCInfo::UNLOCKED
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_DOWN)
	{
		return true;
	}
	return false;
}
bool PLCInfoState::IsEmptyHoistUpOccur() const
{
	if (m_oldPLC.lockStatus == PLCInfo::UNLOCKED
		&& m_oldPLC.hoistStatus != PLCInfo::HOIST_UP
		&& m_curPLC.lockStatus == PLCInfo::UNLOCKED
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_UP)
	{
		return true;
	}
	return false;
}
//带箱起升
bool PLCInfoState::IsContHoistUpOccur() const
{
	if (m_oldPLC.lockStatus == PLCInfo::LOCKED
		&& m_oldPLC.hoistStatus != PLCInfo::HOIST_UP
		&& m_curPLC.lockStatus == PLCInfo::LOCKED
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_UP)
	{
		return true;
	}
	return false;
}
//吊具上升发生
bool PLCInfoState::IsHoistUpOccur() const
{
	if (m_oldPLC.hoistStatus != PLCInfo::HOIST_UP
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_UP)
	{
		return true;
	}
	return false;
}
//吊具下降发生
bool PLCInfoState::IsHoistDownOccur() const
{
	if (m_oldPLC.hoistStatus != PLCInfo::HOIST_DOWN
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_DOWN)
	{
		return true;
	}
	return false;
}

bool PLCInfoState::IsHoistUpStatus() const
{
	if (m_oldPLC.hoistStatus == PLCInfo::HOIST_UP
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_UP)
	{
		return true;
	}
	return false;
}
//移箱
bool PLCInfoState::IsMoveContOccur() const
{
	if ((m_oldPLC.trollyStatus != PLCInfo::FOREWORD
		&& m_oldPLC.lockStatus == PLCInfo::LOCKED
		&& m_curPLC.trollyStatus == PLCInfo::FOREWORD
		&& m_curPLC.lockStatus == PLCInfo::LOCKED)
		|| (m_oldPLC.trollyStatus != PLCInfo::BACKWORD
			&& m_oldPLC.lockStatus == PLCInfo::LOCKED
			&& m_curPLC.trollyStatus == PLCInfo::BACKWORD
			&& m_curPLC.lockStatus == PLCInfo::LOCKED))
	{
		return true;
	}
	return false;
}
//带箱下降
bool PLCInfoState::IsContHoistDownOccur() const
{
	if (m_oldPLC.lockStatus == PLCInfo::LOCKED
		&& m_oldPLC.hoistStatus != PLCInfo::HOIST_DOWN
		&& m_curPLC.lockStatus == PLCInfo::LOCKED
		&& m_curPLC.hoistStatus == PLCInfo::HOIST_DOWN)
	{
		return true;
	}
	return false;
}
// 返回当前PLC信息
const PLCInfo& PLCInfoState::GetCurPLCInfo() const
{
	return m_curPLC;
}
const PLCInfo& PLCInfoState::GetLastPLC() const
{
	return m_oldPLC;
}
// 返回当前识别到的动作
std::string PLCInfoState::GetMovement() const
{
	std::string s;
	if (isCatchOccur()) s += "[着箱产生] ";
	if (isUnCatchOccur()) s += "[着箱消失] ";
	if (isUnLockOccur()) s += "[锁打开] ";
	if (isLockOccur()) s += "[锁关闭] ";
	if (isSpreaderMode()) s += "[吊具尺寸: " + GetLastPLC().GetSpreadMode() + " -> " + m_curPLC.GetSpreadMode() + "] ";
	return s;
}
std::string PLCInfoState::GetMovement(int trollyValue, int hoistValue) const
{
	std::string s = GetMovement();
	if (isTrollyPassValue(trollyValue))
	{
		s += "[小车经过: " + std::to_string(trollyValue) + "] ";
	}
	else if (isHoistPassValue(hoistValue))
	{
		s += "[起升经过: " + std::to_string(hoistValue) + "] ";
	}
	return s;
}
int PLCInfoState::GetHoistAboveValue() const
{
	return hoistAbove;
}
//吊具上升某值发生
bool PLCInfoState::IsHoistAbovePassValueOccur(int value) const
{
	return value >= hoistAboveOld && value <= hoistAbove;
}
bool PLCInfoState::IsPmnResultHaveOccur() const
{
	if (pmmResultStateOld != 1 && pmmResultStateCur ==1 )
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool PLCInfoState::IsCmnResultHaveOccur() const
{
	if (cmmResultStateOld != 1 && cmmResultStateCur == 1)
	{
		return true;
	}
	else
	{
		return false;
	}

}

bool PLCInfoState::IsCpsStatusFinishOccur() const
{
	if (m_oldPLC.cpsStatus != PLCInfo::SEA_CPS_FINISH
		&& m_curPLC.cpsStatus == PLCInfo::SEA_CPS_FINISH)
	{
		return true;
	}
	return false;
}
bool PLCInfoState::IsCpsStatusLeaveOccur() const
{
	if (m_oldPLC.cpsStatus != PLCInfo::SEA_CPS_NO_FINISH
		&& m_curPLC.cpsStatus == PLCInfo::SEA_CPS_NO_FINISH)
	{
		return true;
	}
	return false;
}
//对位距离减小经过某值
bool PLCInfoState::IsCpsDistanceDownPassValue(int value) const
{
	if (m_oldPLC.cpsDistance >= value && m_curPLC.cpsDistance < value )
	{
		if ((m_oldPLC.cpsDistance != 20) && (m_oldPLC.cpsDistance != 10))//屏蔽旧值为CPS默认值20和10的情况防止跳变
		{
			return true;
		}
	}
	return false;
}

// 大车是否移动中
bool PLCInfoState::IsGantryMoving() const
{
	std::lock_guard<std::mutex> lock(g_mtxGantry);
	if (g_queGantry.back()- g_queGantry.front() == 0
		|| m_curPLC.gantrySpeed!=0)
	{
		return false;
	}
	return true;
}
//大车是否移动发生
bool PLCInfoState::IsGantryMoveOccur() const
{
	if (IsGantryHeadMoveOccur()
		|| IsGantryTailMoveOccur())
	{
		return true;
	}
	return false;
}
//大车是否向头间移动发生
bool PLCInfoState::IsGantryHeadMoveOccur() const
{
	std::lock_guard<std::mutex> lock(g_mtxGantry);
	if (g_queGantry.front() -g_queGantry.back()>0)
	{
		console->info("10帧前数据[{}] 当前数据[{}],->头", g_queGantry.front(), g_queGantry.back());
		return true;
	}
	return false;
}
//大车是否向尾间移动发生
bool PLCInfoState::IsGantryTailMoveOccur() const
{
	std::lock_guard<std::mutex> lock(g_mtxGantry);
	if (g_queGantry.front() - g_queGantry.back()<0)
	{
		console->info("10帧前数据[{}] 当前数据[{}],->尾", g_queGantry.front() ,g_queGantry.back());
		return true;
	}
	return false;
}

//大车是否向左移动大于某值发生
bool PLCInfoState::IsGantryLeftMovePassValueOccur(const int value)
{
	//保存上次移动的距离
	int gantryLeftMoveValueOld = gantryLeftMoveValue;
	//向左移动中
	if (m_oldPLC.gantryStatus == PLCInfo::GANTRY_LEFT
		&& m_curPLC.gantryStatus == PLCInfo::GANTRY_LEFT)
	{
		//更新移动距离
		gantryLeftMoveValue += abs(m_curPLC.gantryStatus - m_oldPLC.gantryValue);
	}
	else
	{
		//更新移动距离
		gantryLeftMoveValue = 0;
	}
	return gantryLeftMoveValueOld <= value && gantryLeftMoveValue > value;
}
//大车是否向右移动大于某值发生
bool PLCInfoState::IsGantryRightMovePassValueOccur(const int value)
{
	//保存上次移动的距离
	int gantryRightMoveValueOld = gantryRightMoveValue;
	//向右移动中
	if (m_oldPLC.gantryStatus == PLCInfo::GANTRY_RIGHT
		&& m_curPLC.gantryStatus == PLCInfo::GANTRY_RIGHT)
	{
		gantryRightMoveValue += abs(m_curPLC.gantryStatus - m_oldPLC.gantryValue);
	}
	else
	{
		//更新移动距离
		gantryRightMoveValue = 0;
	}
	return gantryRightMoveValueOld <= value && gantryRightMoveValue > value;
}

//获取大车持续left移动的距离
int PLCInfoState::GetGantryLeftMoveValue() const
{
	return gantryLeftMoveValue;
}

//获取大车持续right移动的距离
int PLCInfoState::GetGantryRightMoveValue() const
{
	return gantryRightMoveValue;
}

//获取大车总共移动的距离 程序启动时为初始值
int PLCInfoState::GetTotalGantryMoveValue() const
{
	return m_curPLC.gantryValue - gantryStartValue;
}

//大车每次移动超过某值
bool PLCInfoState::IsGantryEveryPassValueOccur(int value)
{
	return (m_curPLC.gantryValue - gantryStartValue) / value != (m_oldPLC.gantryValue - gantryStartValue) / value;
}

bool PLCInfoState::IsStartSendResultToPlc() const
{
	//TASKID变为非0
	if (m_curPLC.taskId != m_oldPLC.taskId)
	{
		return true;
	}
	return false;
}

bool PLCInfoState::IsStopSendResultToPlc() const
{
	//TASKID变为0
	if (m_curPLC.taskId == 0 && m_curPLC.taskId != m_oldPLC.taskId)
	{
		return true;
	}
	return false;
}

bool PLCInfoState::DealWithBackContainerOccur() const
{
	if
		(3 == m_curPLC.cpsCnPos && 3 != m_oldPLC.cpsCnPos)
	{
		return true;
	}
	return false;
}

unsigned short PLCInfoState::TaskStatusChangeOccur() const
{
	if (m_curPLC.taskState != m_oldPLC.taskState &&
		m_curPLC.taskState>0)
	{
		return m_curPLC.taskState;
	}
	return m_curPLC.taskState;
}

bool PLCInfoState:: TaskIdChangeOccur() const
{
	if (m_curPLC.taskId != m_oldPLC.taskId &&
		!m_curPLC.taskId.empty())
	{
		return true;
	}
	return false;
}

bool PLCInfoState::CnPosBCountPass(int v)const
{
	if (cnPosBcountCur > v && cnPosBcountOld<=v)
	{
		return true;
	}
	return false;
}

bool PLCInfoState::PlcPacketsCounterForTaskPass(int v) const
{
	if (plcPacketsCounterForTask_cur > v && plcPacketsCounterForTask_old <= v)
	{
		return true;
	}
	return false;
}

//2026.2 对应三一增加车道下是否有车接口，增加判断事件
	//内集卡车道是否有集卡车
bool PLCInfoState ::IsTruckInLane1() const
{
	bool istruckin = (m_curPLC.hasTruck[1] == 1) && (m_oldPLC.hasTruck[1] == 0);
	return istruckin;
}
//外集卡车道是否有集卡车
bool PLCInfoState::IsTruckInLane2()const
{
	bool istruckin = (m_curPLC.hasTruck[2] == 1) && (m_oldPLC.hasTruck[2] == 0);
	return istruckin;
}
//内集卡车道集卡离开是否发生
bool PLCInfoState::IsLane1LeaveOccur() const
{

	return (m_curPLC.hasTruck[1] == 0 && m_oldPLC.hasTruck[1] == 1)|| (m_curPLC.hasTruck[1] == 1&&m_oldPLC.cpsDistance>-5&&m_curPLC.cpsDistance<=-5);
}
//内集卡车道集卡离开是否发生
bool PLCInfoState::IsLane2LeaveOccur() const
{
	return (m_curPLC.hasTruck[2] == 0 && m_oldPLC.hasTruck[2] == 1) || (m_curPLC.hasTruck[2] == 1 && m_oldPLC.cpsDistance >- 5 && m_curPLC.cpsDistance<=- 5);
}
