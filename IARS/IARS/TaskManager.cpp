#include "TaskManager.h"
#include "AppConfig.h"
#include "CDataIntegration.h"
#include "PicSave.h"
#include "DeviceManager.h"
#include "PlcComm.h"
#include "cJSON.h"
#include "EnumTransform.h"
#include "CGRPCRequest.h"
#include "PLCHttpComm.h"
#include <chrono>

std::string GbkToUtf8(std::string& gbk_str) {
	int wLen = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, NULL, 0);
	wchar_t* wStr = new wchar_t[wLen + 1];
	MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, wStr, wLen);
	wStr[wLen] = L'\0';

	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
	char* utf8Str = new char[utf8Len + 1];
	WideCharToMultiByte(CP_UTF8, 0, wStr, -1, utf8Str, utf8Len, NULL, NULL);
	utf8Str[utf8Len] = '\0';

	std::string retStr(utf8Str);
	delete[] wStr;
	delete[] utf8Str;
	return retStr;
}

void TaskManager::SerialSingleRTaskProto(RRIMAGELIST &rImageList)
{
	boost::mutex::scoped_lock lock(taskMutex);
	// 识别不看 m_nState（GUI 心跳，启动后 10s 才 NORMAL）。Start()/Connect ok 后直接发图；
	// 失败由 GetOcrResult 区分：DEADLINE_EXCEEDED→2，stub 未连/其它 RPC 错误→3。
	GRpc_RepeadResult result;
	if (rImageList.empty())
	{
		console->error("待识别图片队列为空");
		return;
	}
	int img_label = rImageList[0]->img_label();
	AddTaskLog(console->info("CRS [{}] 识别开始,image_label: {}", m_task_id, rImageList[0]->img_label()));
	if (!CGRPCRequest::GetInstance()->GetOcrResult(rImageList, result))
	{
		// 发图→回包失败：超时无回包→2；其它通信/调用失败→3。空识别回包不算超时，见下方 result 分支。
		const bool isDeadline = CGRPCRequest::GetInstance()->IsLastErrorDeadlineExceeded();
		if (isDeadline)
		{
			Sany_Code.truck_code = TopCode_TimeoutOnline; // 2：CRS 在线但 CRS_GRPC_WaitTime 内无回包
			AddTaskLog(console->error("CRS 识别超时无回包，TaskId {}，StatusCode {}：{}", m_task_id,
				Sany_Code.truck_code, CGRPCRequest::GetInstance()->GetErrorDetail()));
		}
		else
		{
			Sany_Code.truck_code = TopCode_TimeoutOffline; // 3：非超时类 CRS 调用失败
			Sany_Code.cn_code = ContainerNo_TimeoutOffline;
			Sany_Code.door_code = DoorDir_TimeoutOffline;
			Sany_Code.open_code = DoorOpen_TimeoutOffline;
			AddTaskLog(console->error("CRS 识别调用失败，TaskId {}，StatusCode {}：{}", m_task_id,
				Sany_Code.truck_code, CGRPCRequest::GetInstance()->GetErrorDetail()));
		}
		/*PackStateInfo(StateInfo_ITEM_TYPE_ITEM_COMPUTER, "CRS", grpcReq->GetErrorDetail(),
			StateInfo_RUN_STATE_RUN_FAULT, grpcReq->GetConnectIpStr());*/
		return;
	}

	if (result.result_size() == 0)//无结果
	{
		// 有回包但无条目：不算码 2；最终无车顶号时由 GetPmnResult 写 4
		AddTaskLog(console->error("CRS [{}] 识别无结果（有回包）：{}", m_task_id, CGRPCRequest::GetInstance()->GetErrorDetail()));
		return;
	}
	else
	{
		bool isHaveRes = false;
		for (auto &a : result.result())
		{
			if (a.rtype() != GRpcResult_RESULT_TYPE_T_ZERO)
			{
				isHaveRes = true;
				break;
			}
		}
		if (!isHaveRes)
		{
			// 有回包但全 T_ZERO：同样不算超时，最终空结果由 GetPmnResult 写 4
			AddTaskLog(console->info("CRS [{}] 识别有返回结果，但结果为空", m_task_id));
			return;
		}
		console->info("CRS 识别有返回结果，结果不为空");
	}
	AddTaskLog(console->info("CRS [{}] 识别结果:{}", m_task_id, result.DebugString()), true);

	tCrsRet crs_ret;
	crs_ret.img_label = img_label;
	crs_ret.img_type = rImageList[0]->img_type();
	crs_ret.pGRpcRet = std::make_shared<GRpc_RepeadResult>(result);
	AddTaskLog(console->info("[CRS] 保存识别结果 img_label={}", img_label));
	m_CrsRetList.add(crs_ret);
}

TaskManager::TaskManager()
    : m_task_id("")
    , m_lane_id("")
    , m_spreader("")
	, m_isContinueTask(false)
	, m_taskState(TASKEND)
	, m_isSavePlcInfo(false)
	, m_nLockType(LOCK_NONE)
	, m_truckNumSuccess(PLCInfo::RESULT_TYPE_INVALID)
	, m_cnNumSuccess(PLCInfo::RESULT_TYPE_INVALID)
	,m_truckType (PLCInfo::TRUCK_TYPE_INVALID)
	,m_spreaderMode (PLCInfo::SPREADER_INVALID)
	, m_gantryAnchor(-9999)     // 进窗时大车编码器
{
	g_SavePath = g_AppConfig.userCfg.savePath;
	m_bIsSendingResult = false;
	m_pComposeResult = std::make_shared<ComposeResult>(this);
	m_pDataIntegration = std::make_shared<CDataIntegration>(this);

	// 初始化上一帧各车道是否有效窗口
	memset(m_pmnWindowInLane, 0, sizeof(m_pmnWindowInLane));
	memset(m_hasTruckZeroFrames, 0, sizeof(m_hasTruckZeroFrames));
	memset(cur_truckStatus, 0, sizeof(cur_truckStatus));
	memset(old_truckStatus, 0, sizeof(old_truckStatus));
	for (int i = 0; i < 3; ++i)
	{
		m_pendingPlate[i].Clear();
	}

    //根据配置参数，创建路径并且检测磁盘容量
	MakeDirs(g_AppConfig.userCfg.savePath);
	MakeEnoughSpace();
	CGRPCRequest::GetInstance(); // 拉起 CRS 单例与 GUI 心跳线程；识别见 SerialSingleRTaskProto
}

TaskManager::~TaskManager()
{
    exitFlag = true;
    listener.detach();
}

bool TaskManager::SimilarMatchOnce(PmnResult &pmn,const string &baseStr)
{
	if (pmn.pmn.length() == 3)
	{
		if (baseStr == pmn.pmn.substr(0,2) || baseStr == pmn.pmn.substr(1, 2))
		{
			return true;
		}
	}
	else
	{
		if (baseStr == pmn.pmn)
		{
			return true;
		}

	}

	return false;
}

bool TaskManager::SimilarMatchPmn(const string &baseStr)
{
	if (m_CRSPmn && m_IARSPmn)
	{
		if (SimilarMatchOnce(m_IARSInnerPmn, baseStr)
			|| SimilarMatchOnce(m_IARSOuterPmn, baseStr)
			|| SimilarMatchOnce(m_CRSInnerPmn, baseStr)
			|| SimilarMatchOnce(m_CRSOuterPmn, baseStr))
		{
			return true;
		}
	}
	else if (m_CRSPmn)
	{
		if (SimilarMatchOnce(m_CRSInnerPmn, baseStr)
			|| SimilarMatchOnce(m_CRSOuterPmn, baseStr))
		{
			return true;
		}
	}
	else if (m_IARSPmn)
	{
		if (SimilarMatchOnce(m_IARSInnerPmn, baseStr)
			|| SimilarMatchOnce(m_IARSOuterPmn, baseStr))
		{
			return true;
		}
	}
	return false;
}

bool TaskManager::SimilarMatch()
{
	bool bSimilarMatch = false;
	//内集卡计划车号长度是4
	if (m_planTruckNum.length() == 4)
	{
		//2位数字 比对最后2位
		if (m_planTruckNum[1] == '0')
		{
			string planTruckNumSubStrA = m_planTruckNum.substr(2, 2);
			bSimilarMatch = SimilarMatchPmn(planTruckNumSubStrA);
		}
		else
		{
			string planTruckNumSubStrF = m_planTruckNum.substr(1, 2);
			string planTruckNumSubStrA = m_planTruckNum.substr(2, 2);

			bSimilarMatch = SimilarMatchPmn(planTruckNumSubStrF) || SimilarMatchPmn(planTruckNumSubStrA);
		}
	}
	//外集卡计划车号长度是3
	else if (m_planTruckNum.length() == 3)
	{
		string planTruckNumSubStrF = m_planTruckNum.substr(0, 2);
		string planTruckNumSubStrA = m_planTruckNum.substr(1, 2);

		bSimilarMatch = SimilarMatchPmn(planTruckNumSubStrF) || SimilarMatchPmn(planTruckNumSubStrA);
	}
	return bSimilarMatch;
}

string TaskManager::ComposePmnResult(int truckType, int spreaderMode, bool &bMatch)
{
	AddTaskLog(console->info("[send][pmn] [compose] 计划集卡类型[{}](1=inner 2=outer) 计划车号[{}]", truckType, m_planTruckNum));

	string sendPmn = "";

	string pmn_iars = m_IARSInnerPmn.pmn;
	string pmn_crs = m_CRSInnerPmn.pmn;
	string pmnout_iars = m_IARSOuterPmn.pmn;
	string pmnout_crs = m_CRSOuterPmn.pmn;
	GetTruckNo(pmn_iars);
	GetTruckNo(pmn_crs);
	GetTruckNo(pmnout_iars);
	GetTruckNo(pmnout_crs);
	if (m_CRSPmn && m_IARSPmn)
	{
		if (m_planTruckNum == pmn_iars|| m_planTruckNum == pmn_crs || m_planTruckNum == pmnout_iars || m_planTruckNum == pmnout_crs)
		{
			AddTaskLog(console->info("[send][pmn] [compose] 计划车号[{}] IARS有结果 CRS有结果 匹配成功", m_planTruckNum));
			bMatch = true;
		}
	}
	else if(m_CRSPmn)
	{
		if (m_planTruckNum == pmn_crs || m_planTruckNum == pmnout_crs)
		{
			AddTaskLog(console->info("[send][pmn] [compose] 计划车号[{}] CRS有结果 匹配成功", m_planTruckNum));
			bMatch = true;
		}
	}
	else if (m_IARSPmn)
	{
		if (m_planTruckNum == pmn_iars || m_planTruckNum == pmnout_iars)
		{
			AddTaskLog(console->info("[send][pmn] [compose] 计划车号[{}] IARS有结果 匹配成功", m_planTruckNum));
			bMatch = true;
		}
	}
	//模糊搜索
	if (!bMatch)
	{
		//如果使用模糊匹配
		if (g_AppConfig.userCfg.pmnSimilarMatch)
		{
			bMatch = SimilarMatch();
			if (bMatch)
			{
				WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_similar_" + m_planTruckNum + ".txt", m_planTruckNum);
			}
			if (m_IARSPmn)
			{
				AddTaskLog(console->info("[send][pmn] [compose] [similar] IARS结果模糊匹配 inner[{}] outer[{}] 计划车号[{}]",m_IARSInnerPmn.pmn,m_IARSOuterPmn.pmn, m_planTruckNum));
			}
			if (m_CRSPmn)
			{
				AddTaskLog(console->info("[send][pmn] [compose] [similar] CRS结果模糊匹配 inner[{}] outer[{}] 计划车号[{}]", m_CRSInnerPmn.pmn, m_CRSOuterPmn.pmn, m_planTruckNum));
			}
			AddTaskLog(console->info("[send][pmn] [compose] [similar] 模糊匹配 计划车号[{}] 匹配结果[{}]", m_planTruckNum, bMatch));
		}
	}
	//不匹配，根据计划集卡类型使用结果
	if (!bMatch)
	{
		//AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 根据计划集卡类型[{}](1=inner 2=outer) 复核结果", truckType));
		if (truckType == PLCInfo::TRUCK_TYPE_INNER)
		{
			if (m_CRSPmn && m_IARSPmn)
			{
				sendPmn = m_IARSInnerPmn.conf > m_CRSInnerPmn.conf ? m_IARSInnerPmn.pmn : m_CRSInnerPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[内] IARS有结果 CRS有结果 复核结果 iars[in:{} conf:{}] crs[in:{} conf:{}] 保留置信度高的[{}]",
					m_IARSInnerPmn.pmn, m_IARSInnerPmn.conf,m_CRSInnerPmn.pmn,m_CRSInnerPmn.conf,sendPmn));
			}
			else if (m_CRSPmn)
			{
				sendPmn = m_CRSInnerPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[内] CRS有结果 使用内结果 :[{}]",
					sendPmn));
			}
			else if (m_IARSPmn)
			{
				sendPmn = m_IARSInnerPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[内] IARS有结果 使用内结果 :[{}]",
					sendPmn));
			}
			else
			{
				if (m_prePmn.spreaderMode == PLCInfo::SINGLE20 && spreaderMode == PLCInfo::SINGLE20)
				{
					sendPmn = m_prePmn.pmn;
					AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[内] 都没有结果 前吊是20尺 本次也是20尺 前吊具类型:{} 后吊具类型:{} 使用上次结果:{}", m_prePmn.spreaderMode, spreaderMode, sendPmn));
				}
				else
				{
					AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[内] 都没有结果 不符合前后都是20尺 前吊具类型:{} 后吊具类型:{} 无车号", m_prePmn.spreaderMode, spreaderMode));
				}
			}
		}
		else if (truckType == PLCInfo::TRUCK_TYPE_OUTER)
		{
			if (m_CRSPmn && m_IARSPmn)
			{
				sendPmn = m_IARSOuterPmn.conf > m_CRSOuterPmn.conf ? m_IARSOuterPmn.pmn : m_CRSOuterPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[外] IARS有结果 CRS有结果 复核结果 iars[out:{} conf:{}] crs[out:{} conf:{}] 保留置信度高的[{}]",
					m_IARSOuterPmn.pmn, m_IARSOuterPmn.conf, m_CRSOuterPmn.pmn, m_CRSOuterPmn.conf, sendPmn));
			}
			else if (m_CRSPmn)
			{
				sendPmn = m_CRSOuterPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[外] CRS有结果 使用外结果 [{}]",
					sendPmn));
			}
			else if (m_IARSPmn)
			{
				sendPmn = m_IARSOuterPmn.pmn;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[外] IARS有结果 使用外结果 [{}]",
					sendPmn));
			}
			else
			{
				if (m_prePmn.spreaderMode == PLCInfo::SINGLE20 && spreaderMode == PLCInfo::SINGLE20)
				{
					sendPmn = m_prePmn.pmn;
					AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[外] 都没有结果 前吊是20尺 本次也是20尺 前吊具类型:{} 后吊具类型:{} 使用上次结果:{}", m_prePmn.spreaderMode, spreaderMode, sendPmn));
				}
				else
				{
					AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[外] 都没有结果 不符合前后都是20尺 前吊具类型:{} 后吊具类型:{} 无车号", m_prePmn.spreaderMode, spreaderMode));
				}
			}
		}
		else//未知，置信度高的
		{
			//先默认内集卡
			sendPmn = m_CRSInnerPmn.pmn;

			float conf = m_CRSInnerPmn.conf;
			AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[未知] 默认使用crs内结果pmn[{}] conf[{}]", sendPmn, conf));
			if (m_IARSInnerPmn.conf > conf)
			{
				sendPmn = m_IARSInnerPmn.pmn;
				conf = m_IARSInnerPmn.conf;
				AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[未知] IARS内结果置信度高替换pmn[{}] conf[{}]", sendPmn, conf));
			}

			if (!sendPmn.empty())
			{
				//如果车号结果含有字母，认为是外集卡
				if (!IsAllNumber(sendPmn))
				{
					//外集卡，置信度高的，默认先使用crs结果
					sendPmn = m_CRSOuterPmn.pmn;
					conf = m_CRSOuterPmn.conf;
					AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[未知] 车号含有字母 改为crs外集卡结果pmn[{}] conf[{}]", sendPmn, conf));
					if (m_IARSOuterPmn.conf > conf)
					{
						sendPmn = m_IARSOuterPmn.pmn;
						conf = m_IARSOuterPmn.conf;
						AddTaskLog(console->info("[send][pmn] [compose] 完全匹配和模糊匹配都未成功 集卡类型[未知] 车号含有字母 IARS外集卡结果置信度高pmn[{}] conf[{}]", sendPmn, conf));
					}
				}
			}

		}
	}
	//匹配直接发送计划车号
	else
	{
		sendPmn = m_planTruckNum;
		AddTaskLog(console->info("[send][pmn] [compose] 与计划车号匹配成功 使用[{}]",sendPmn));
	}

	return sendPmn;
}
void TaskManager::TaskTrigger(const PLCInfo &plcInfo)
{
	m_plcInfoState.Update(plcInfo);

	// 更新车顶号入库门控进出（无车出窗才可能清池；大车移动不清路过票）
	UpdatePmnWindowState();

	if (m_plcInfoState.TaskIdChangeOccur())
	{
		console->warn(" ******* 作业已结束taskid[{}]->[{}] *******", m_plcInfoState.GetLastPLC().taskId, m_plcInfoState.GetCurPLCInfo().taskId);

		// 作业结束前先发送OcrData
		if (m_plcInfoState.GetLastPLC().taskId !="0")
		{
			SendResult();
		}
		DealEventTaskEnd();

		m_taskState = TASKEND;
		m_task_id.clear();
		m_planTruckMatch = false;
		m_planCnMatch = false;
		m_planTruckNum.clear();
		m_lockGantryValue = -9999;
		m_lockTrallyValue = -9999;
		m_lockHoistValue = -9999;
		m_isContinueTask = false;
		m_mtxCnItem.lock();
		m_arrCnItem.clear();
		m_mtxCnItem.unlock();

		Sany_Code.truck_code = Truck_Code:: Normal_FromPmn;
		Sany_Code.cn_code = CN_Code::ContainerNo_Normal;
		Sany_Code.door_code = DoorDir_Normal;
		Sany_Code.open_code = DoorOpen_Normal;

		m_mtxPmnItem.lock();
		m_arrPmnItem.clear();
		//m_arrPmnItem2.clear();
		m_mtxPmnItem.unlock();

		m_bDoorOpenResult = 0;
		m_mtxCamDoorRet.lock();
		m_mapCamDoorRet.clear();
		m_mtxCamDoorRet.unlock();

		m_reqPmnCount = 0;
		UnInitTaskData();

		//清理磁盘空间
		MakeEnoughSpace();

		//////////////////////////////////////////
		m_lockGantryValue = -9999;
		m_lockTrallyValue = -9999;
		m_lockHoistValue = -9999;
		m_isContinueTask = false;
		m_taskState = TASKSTART;
		m_mtxCnItem.lock();
		m_arrCnItem.clear();
		m_mtxCnItem.unlock();

		m_mtxPmnItem.lock();
		m_arrPmnItem.clear();
		//m_arrPmnItem2.clear();
		m_mtxPmnItem.unlock();

		m_bDoorOpenResult = 0;
		m_mtxCamDoorRet.lock();
		m_mapCamDoorRet.clear();
		m_mtxCamDoorRet.unlock();

		InitTaskData();
		//任务开始
		SetTaskID();
		//作业完成状态和图片ftp路径
		m_task_dir = GetDateString() + "\\" + m_task_id;
		m_TaskInfo.set_snapshot_location(m_task_dir);
		m_TaskInfo.set_local_snapshot_location(g_AppConfig.userCfg.savePath + "\\" + m_task_dir + "\\");
		MakeDirs(m_TaskInfo.local_snapshot_location());

		m_tRetInfo.jobid = m_task_id;
		m_tRetInfo.quayNo = g_AppConfig.userCfg.quayNo;
		m_tRetInfo.startTime = GetTimeString();

		m_eventSet.clear();
		auto strInfo = console->warn("[{}] ******* 作业开始 ******* {}", m_task_id, plcInfo.taskId);
		AddTaskLog(strInfo);
		AddMainLog(strInfo);

		//作业开始 是否寄桥初始化
		m_TaskInfo.set_stay_work(0);
		SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_TASK_BEGIN, "EVENT_TASK_BEGIN",
			m_plcInfoState.GetCurPLCInfo().gantryValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue);
		m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_BEGIN;

		ShowEvents();


	}
	//if (m_taskState == TASKEND && (plcInfo.taskState != 1))
	//{
	//	//console->warn(" ******* 无作业{} *******", plcInfo.taskState);
	//	return;
	//}
	//if (m_taskState == TASKSTART && (plcInfo.taskState == 91 || plcInfo.taskState == 99 || plcInfo.taskState == 0))
	//{
	//	console->warn(" ******* 作业已结束或丢弃或无效{} *******", plcInfo.taskState);
	//	DealEventTaskEnd();

	//	m_taskState = TASKEND;
	//	m_task_id.clear();
	//	m_planTruckMatch = false;
	//	m_planTruckNum.clear();
	//	m_lockGantryValue = -9999;
	//	m_lockTrallyValue = -9999;
	//	m_lockHoistValue = -9999;
	//	m_isContinueTask = false;
	//	m_mtxCnItem.lock();
	//	m_arrCnItem.clear();
	//	m_mtxCnItem.unlock();

	//	m_mtxPmnItem.lock();
	//	m_arrPmnItem.clear();
	//	m_arrPmnItem2.clear();
	//	m_mtxPmnItem.unlock();

	//	m_bDoorOpenResult = 0;
	//	m_mtxCamDoorRet.lock();
	//	m_mapCamDoorRet.clear();
	//	m_mtxCamDoorRet.unlock();

	//	m_reqPmnCount = 0;
	//	UnInitTaskData();

	//	//清理磁盘空间
	//	MakeEnoughSpace();
	//	return;
	//}

	//if (m_taskState == TASKEND && plcInfo.taskState == 1)//任务开始
	//{
	//	m_lockGantryValue = -9999;
	//	m_lockTrallyValue = -9999;
	//	m_lockHoistValue = -9999;
	//	m_isContinueTask = false;
	//	m_taskState = TASKSTART;
	//	m_mtxCnItem.lock();
	//	m_arrCnItem.clear();
	//	m_mtxCnItem.unlock();

	//	m_mtxPmnItem.lock();
	//	m_arrPmnItem.clear();
	//	m_arrPmnItem2.clear();
	//	m_mtxPmnItem.unlock();

	//	m_bDoorOpenResult = 0;
	//	m_mtxCamDoorRet.lock();
	//	m_mapCamDoorRet.clear();
	//	m_mtxCamDoorRet.unlock();

	//	InitTaskData();
	//	//任务开始
	//	SetTaskID();
	//	//作业完成状态和图片ftp路径
	//	m_task_dir = GetDateString() + "\\" + m_task_id;
	//	m_TaskInfo.set_snapshot_location(m_task_dir);
	//	m_TaskInfo.set_local_snapshot_location(g_AppConfig.userCfg.savePath + "\\" + m_task_dir + "\\");
	//	MakeDirs(m_TaskInfo.local_snapshot_location());

	//	m_tRetInfo.jobid = m_task_id;
	//	m_tRetInfo.quayNo = g_AppConfig.userCfg.quayNo;
	//	m_tRetInfo.startTime = GetTimeString();

	//	m_eventSet.clear();
	//	auto strInfo = console->warn("[{}] ******* 作业开始 ******* {}", m_task_id, plcInfo.taskId);
	//	AddTaskLog(strInfo);
	//	AddMainLog(strInfo);
	//
	//	//作业开始 是否寄桥初始化
	//	m_TaskInfo.set_stay_work(0);
	//	SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_TASK_BEGIN, "EVENT_TASK_BEGIN",
	//		m_plcInfoState.GetCurPLCInfo().gantryValue,
	//		m_plcInfoState.GetCurPLCInfo().hoistValue,
	//		m_plcInfoState.GetCurPLCInfo().trollyValue);
	//	m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_BEGIN;

	//	ShowEvents();
	//}
	if (m_bRecvJobTriggerSendLog)
	{
		console->info("车道[{}]接收到一帧jobtrigger id:{},位置g:{} t:{} h:{} Lock:{}",
			m_lane_id,
			m_JobTrigger.body().job_id(),
			m_plcInfoState.GetCurPLCInfo().gantryValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue,
			m_plcInfoState.GetCurPLCInfo().lockStatus);
		m_bRecvJobTriggerSendLog = false;
	}
	if (m_plcInfoState.IsGantryTailMoveOccur())
	{

		AddTaskLog(console->info("触发大车向尾间移动事件，外集卡车牌数据,arr2 size[{}]",m_arrPmnItem2.size()));

	}
	if (m_plcInfoState.IsCpsStatusFinishOccur())
	{
		DealEventTruckReady();
	}
	// 不再用视觉 TruckStatus==离开(2) 清车顶号/车牌，改由 UpdatePmnWindowState 按 hasTruck 消抖后 ClearPmnCache
	//if (/*m_plcInfoState.IsLane1LeaveOccur()*/ old_truckStatus[1]!=2&&cur_truckStatus[1]==2)
	//{
	//	DealEventTruckLeave(1);
	//	old_truckStatus[1] = 2;
	//}
	//if (/*m_plcInfoState.IsLane2LeaveOccur()*/old_truckStatus[2] != 2 && cur_truckStatus[2] == 2)
	//{
	//	DealEventTruckLeave(2);
	//	old_truckStatus[2] = 2;
	//}
	if (m_plcInfoState.IsCpsDistanceDownPassValue(40))
	{
		AddTaskLog(console->info("车道[{}]触发 [EVENT_DISTANCE_PASS] 事件", m_lane_id));
		//开始保存Plc
		if (!m_isSavePlcInfo)
		{
			CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, m_plcInfoState.GetCurPLCInfo());
			m_isSavePlcInfo = true;
			console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
		}
	}

/*	if(m_plcInfoState.TaskStatusChangeOccur()== PLCInfo::TASK_STATE_START)
	{
		m_taskState = TASKSTART;

	} */

	if (m_plcInfoState.isLockOccur())
	{
		//发生闭锁
		DealEventLock();
	}

	if (m_plcInfoState.isUnLockOccur())
	{
		//发生开锁
		DealEventUnLock();

		//m_taskState = TASKEND;
		//string strInfo = console->warn("[pmn][cn][task]开锁发生，作业结束",
		//	m_lane_id, m_plcInfoState.GetLastPLC().hoistValue, plcInfo.hoistValue, g_AppConfig.userCfg.reset_dooropen_by_hoist_value);
		//AddTaskLog(strInfo);
		//AddMainLog(strInfo);
		m_tRetInfo.endTime = GetTimeString();

		SaveCSV(m_tRetInfo.to_string());
	}


	if (!plcInfo.truckNumStr.empty())
	{
		if (plcInfo.truckNumStr != m_planTruckNum)
		{
			m_planTruckNum = plcInfo.truckNumStr;
			m_truckType = plcInfo.truckType;
			AddTaskLog(console->info("[plan][pmn] 接收到计划车号 [{}] type[{}]", m_planTruckNum, m_truckType));

			WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_plan_" + m_planTruckNum+".txt", m_planTruckNum);

			m_tRetInfo.tosTruck = m_planTruckNum;
		}
	}
	if (plcInfo.lockStatus == PLCInfo::UNLOCKED &&
		m_plcInfoState.isHoistPassValue(g_AppConfig.userCfg.reset_dooropen_by_hoist_value))
	{
		g_mtxOCRToPlc.lock();
		if (g_OCRToPlc.doorOpen != OCRToPlc::DOOR_NONE)
		{
			AddTaskLog(console->info("空吊具，高度[{}->{}]经过{}复位箱门状态{}为NONE", m_plcInfoState.GetLastPLC().hoistValue, plcInfo.hoistValue, g_AppConfig.userCfg.reset_dooropen_by_hoist_value, g_OCRToPlc.GetDoorOpen()));
			g_OCRToPlc.doorOpen = OCRToPlc::DOOR_NONE;
		}
		g_mtxOCRToPlc.unlock();

		m_mtxTaskInfo.lock();
		m_mapCamOpenRet.clear();
		m_mtxTaskInfo.unlock();
	}
	/*
	当吊具到箱顶距离向上经过配置值时，清空缓存的箱号结果与箱号图片缓存（不删本地文件）。
	存在吊具摄像头多次抓箱识别多个箱号：第一次尝试抓箱放弃后，再下降到该高度会重新拍照识别，
	二次复核应只使用本轮识别结果（不再依赖与历史结果比“最新时间”）。
	*/
	if (g_AppConfig.userCfg.hoistToContUpVal > 0 &&
		m_plcInfoState.IsHoistToContPassValueUp(g_AppConfig.userCfg.hoistToContUpVal))
	{
	    // 吊具提升经过2m，清空箱号结果和箱号图片
		ClearCnResultOnHoistToContUp();
	}
	//if (m_plcInfoState.isHoistPassValueUp(g_AppConfig.userCfg.reset_dooropen_by_hoist_value))
	//{
	//	for (auto &item : m_truckNumDeque)
	//	{
	//		item.isSend = false;
	//	}
	//	//清空
	//	g_mtxOCRToPlc.lock();
	//	g_OCRToPlc.Clear();
	//	g_mtxOCRToPlc.unlock();
	//	m_bIsSendingResult = false;
	//	m_IARSPmn = false;
	//	m_CRSPmn = false;
	//	m_TruckNumResultNeed = false;
	//	m_TruckResultSend = false;
	//	m_CNResultNeed = false;
	//	m_CNResultSend = false;
	//	m_StartSendPmnTime = GetCurrentStamp64();
	//	m_IARSInnerPmn.Clear();
	//	m_IARSOuterPmn.Clear();
	//	m_CRSInnerPmn.Clear();
	//	m_CRSOuterPmn.Clear();

	//	m_truckNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	//	m_cnNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	//	m_truckType = PLCInfo::TRUCK_TYPE_INVALID;
	//	m_spreaderMode = PLCInfo::SPREADER_INVALID;

	//	m_mtxTaskInfo.lock();
	//	m_mapCamOpenRet.clear();
	//	m_mtxTaskInfo.unlock();

	//	/*m_taskState = TASKEND;
	//	string strInfo = console->warn("[pmn]开锁发生，作业结束",
	//		m_lane_id, m_plcInfoState.GetLastPLC().hoistValue, plcInfo.hoistValue, g_AppConfig.userCfg.reset_dooropen_by_hoist_value);
	//	AddTaskLog(strInfo);
	//	AddMainLog(strInfo);*/
	//}
	if (m_bIsSendingResult && m_CNResultNeed && m_nUpdateResult>0)
	{

		m_nUpdateResult = 0;
		SendResult(true);

		m_tRetInfo.cnTime = GetTimeString();

		m_CNResultSend = true;
		unsigned char szWriteBuf[100] = {};
		g_mtxOCRToPlc.lock();
		string strWritePlc = g_OCRToPlc.to_string();
		int nWriteLen = g_OCRToPlc.SeriralToBuffer(szWriteBuf);
		g_mtxOCRToPlc.unlock();

		string strLog = console->info("UpdateRetToPlc {} {}", strWritePlc, BufToHex(szWriteBuf, nWriteLen));
		AddTaskLog(strLog);
		//WriteStringToTxt(m_strJobPath, strLog);
		AddMainLog(strLog);
		AddMainLog("---------------------------------------\n");
		WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_SendToPlc.txt", strLog);
	}

	//console->warn("----- {}------{}", m_plcInfoState.GetLastPLC().taskId, m_plcInfoState.GetCurPLCInfo().taskId);
	//if (m_plcInfoState.GetLastPLC().taskId)
	//{
	//	console->warn("----- {}", plcInfo);

	//}
	//状态变为开始给PLC发送数据
	//更新时间
	if (!m_TruckNumResultNeed)
	{
		m_StartSendPmnTime = GetCurrentStamp64();
	}
	//要车号结果且没发送过结果
	if (m_TruckNumResultNeed && !m_TruckResultSend)
	{
		string pmn = "";
		//超时发送

		bool bMatch = false;
		if ((GetCurrentStamp64() - m_StartSendPmnTime) > g_AppConfig.userCfg.sendTruckTimeOut)
		{
			AddTaskLog(console->info("[send][pmn] plc请求车号结果[{}] 超时发送 iars结果接收[{}] crs结果接收[{}]",m_planTruckNum,m_IARSPmn,m_CRSPmn));

			pmn = ComposePmnResult(plcInfo.truckType,plcInfo.spreaderMode, bMatch);
			//超时没有结果，在等待一段时间
			if (pmn.empty())
			{
				m_StartSendPmnTime = GetCurrentStamp64();
			}
			else
			{
				m_TruckResultSend = true;
			}
		}
		//有结果
		if (m_TruckResultSend && !pmn.empty())
		{
			if (!bMatch)
			{
				GetTruckNo(pmn);
				AddTaskLog(console->info("[send][pmn] 超时发送车号结果[{}] 未模糊匹配", pmn));
			}
			else
			{
				AddTaskLog(console->info("[send][pmn] 超时发送车号结果[{}] 匹配成功", pmn));
			}

			g_mtxOCRToPlc.lock();
			g_OCRToPlc.strTruckNo = pmn;
			//保存本次作业结果及吊具类型
			m_prePmn.pmn = pmn;
			m_prePmn.spreaderMode = plcInfo.spreaderMode;

			if (m_CNResultSend)
			{
				g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_ALL;
			}
			else
			{
				g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_PMN;
			}
			unsigned char szWriteBuf[100] = {};

			string strWritePlc = g_OCRToPlc.to_string();
			int nWriteLen = g_OCRToPlc.SeriralToBuffer(szWriteBuf);
			g_mtxOCRToPlc.unlock();
			m_tRetInfo.truckTime = GetTimeString();
			string strLog = console->info("[pmn] UpdateRetToPlc {} {}", strWritePlc, BufToHex(szWriteBuf, nWriteLen));
			AddTaskLog(strLog);
			//WriteStringToTxt(m_strJobPath, strLog);
			AddMainLog(strLog);
			AddMainLog("---------------------------------------\n");
			WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_SendToPlc.txt", strLog);
		}

	}

	if (m_isSavePlcInfo)
	{
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location()+m_task_id, m_plcInfoState.GetCurPLCInfo());
	}

	string eventsStr = ShowEvents(false);
	m_TaskInfo.set_event(eventsStr);

	int64_t endT = 0;
	GetCurrentStamp64(endT);
	m_TaskInfo.set_cur_time(endT);//更新时间

	//任务数据保存本地
	//WriteProtoToLocalFile(m_taskSavePath + m_task_id, m_TaskInfo);
}

//发送结果给消息代理
void TaskManager::SendResult(bool isSendReleaseTruck)
{
	console->info("车道[{}]发送OCR结果,最新事件：{}", m_lane_id, m_eventType);
	//数据整合
	m_pDataIntegration->DataIntegration(m_OcrData);

	//组包
	OCRAny any;
	any.mutable_any()->PackFrom(m_OcrData);
	any.set_msg_id(GetUUID());
	any.set_msg_type(MSG_TYPE::MSG_OCRDATA);
	any.set_src_id(g_AppConfig.userCfg.IARSName);
	any.set_time(GetTimeString());
	any.set_need_reply(1);
	any.set_dev_id(g_AppConfig.userCfg.quayNo);
	auto strLog = console->info("车道[{}]复合结果 \n{}", m_lane_id, ConvertChs(any.DebugString()));
	AddTaskLog(strLog);
	//WriteStringToTxt(m_strJobPath, GetTimeString() + " ");
	//WriteStringToTxt(m_strJobPath, strLog);
	AddMainLog(strLog);
	AddMainLog("---------------------------------------\n");
	//发送结果到后台
	g_WaitPubMsg.add(any);

	//从g_arrPicItem检查需要删除的item
	time_t tNow = time(nullptr);
	g_mtxPicItem.lock();
	vector<tPicItem>::iterator iter;
	for (iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
	{
		//数据已经处理，删除
		if (iter->crs_deal &&
			iter->cam_name.length())//cam_name为空，表示车牌相机采集的图片，同一个作业车牌相机图片可能使用多次，在开锁时进行删除。
		{
			AddTaskLog(console->info("SendResult:数据处理完成: task_id={} img_label={} ori_name={}",iter->task_id, iter->img_label, iter->ori_name));
			iter = g_arrPicItem.erase(iter);
		}
		//超时30分钟，删除
		else if (tNow - iter->tCreate >= 30*60)
		{
			AddTaskLog(console->info("SendResult:数据处理超时: task_id={} img_label={} ori_name={}", iter->task_id, iter->img_label, iter->ori_name));
			iter = g_arrPicItem.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	AddTaskLog(console->info("SendResult:数据处理完成:  g_arrPicItem_size={}", g_arrPicItem.size()), true);
	g_mtxPicItem.unlock();

	////发送完成OCRDATA后清空m_TaskInfo中保存的本次作业相关信息
	//m_mtxTaskInfo.lock();
	//AddTaskLog(console->info("清空TaskInfo: {}", ConvertChs(m_TaskInfo.DebugString())));
	//m_TaskInfo.Clear();
	//m_TaskInfo.set_truck_head_dir("Unknow");
	//m_TaskInfo.set_work_status(WS_UNFINISH);
	//m_TaskInfo.set_work_type(WorkTypeUnknow);
	//m_TaskInfo.set_pmn_result(0); //车号结果状态：0：无结果、1：有结果，没通过检验、2：有结果，校验通过
	//m_TaskInfo.set_cmn_result(0);
	//m_TaskInfo.set_work_id(m_task_id);
	//m_TaskInfo.set_dev_id(g_AppConfig.userCfg.quayNo);
	//m_TaskInfo.set_snapshot_location(m_task_dir);
	//m_TaskInfo.set_local_snapshot_location(g_AppConfig.userCfg.savePath + "\\" + m_task_dir + "\\");
	//m_mtxTaskInfo.unlock();
	////将接收到的箱号识别结果清空
	//m_mtxCnItem.lock();
	//m_arrCnItem.clear();
	//m_mtxCnItem.unlock();
}

void TaskManager::Start(string side, string laneid, string cfg_file)
{
    console->info("任务管理<模块启动>");

	if (!m_LaneCfg.load(cfg_file))
	{
		console->error("解析 Lanes 失败 {}", cfg_file);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		exit(1);
	}
	if (!m_posCfg.load(cfg_file))
	{
		console->error("解析 Cam Position 失败 {}", cfg_file);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		exit(1);
	}
	g_PositionCfgMutex.lock();
	for (auto &item : m_posCfg.m)
	{
		if (g_PositionCfg.m.find(item.first) == g_PositionCfg.m.end())
		{
			g_PositionCfg.m.insert(make_pair(item.first, item.second));
		}
	}
	for (auto &item : g_PositionCfg.m)
	{
		console->info("Camera Position cfg cam:{} pos:{}(1=TruckHead,2=TruckTail)", item.first, item.second);
	}
	g_PositionCfgMutex.unlock();
	if (!m_TaskEventCfg.load(cfg_file))
	{
		console->error("解析 Task 失败 {}", cfg_file);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		exit(1);
	}
	//将m_LaneCfg配置的车道数据保存到全局变量g_LaneCfg中
	for (auto&t : m_LaneCfg.m)
	{
		if (g_LaneCfg.m.find(t.first) != g_LaneCfg.m.end())
		{
			console->warn("车道[{}]重复，范围[{},{}]替换为[{},{}]",
				t.first, g_LaneCfg.m[t.first].minValue, g_LaneCfg.m[t.first].maxValue, t.second.minValue, t.second.maxValue);
		}
		g_LaneCfg.m[t.first] = t.second;
	}

	m_lane_id = laneid;
	m_pDevMgr = std::make_shared<CDeviceManager>();
	m_pDevMgr->Start(side, laneid, cfg_file, this);

	exitFlag = false;
    worker = std::thread
    {
        [](TaskManager *task)
        {
            while (!task->exitFlag)
            {
                // 轮寻找识别任务队列
                RRIMAGELIST rImage;
				task->m_RImageList.get(rImage);
				console->info("CRS [{}] 识别图片数{}", task->m_task_id, rImage.size());
				// 取照片数据
				task->SerialSingleRTaskProto(rImage);
                SleepMilliseconds(1);
            };
        }, this
    };
    // 通过PLC判断作业状态
    listener = std::thread
    {
        [](TaskManager *task)
        {
            PLCInfo plcInfo;
           	//每个TaskManager注册PLC时需设置不同的id，否则第二个TaskManager获取不到PLC信息
			string plc_id = "TaskPlc_" + task->m_lane_id;
            SubscribePLC(plc_id, plcInfo);
			while (1)
            {
                // 等待PLC更新消息
                boost::unique_lock < boost::mutex> lck(g_PlcUpdateMutex);
                g_PlcUpdateCond.wait(lck);
                lck.unlock();
				/*
				//未连接PLC，测试用代码，间隔500ms调用TaskTrigger
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				*/
				task->m_plcinfoMtx.lock();
				task->m_plcinfo = plcInfo;
				task->m_plcinfoMtx.unlock();
                task->TaskTrigger(plcInfo);
				//bool bIsShow = PCI_IsShow();
            };
            UnSubscribePLC(plc_id);
        }, this
    };

	std::thread thImgDeal = std::thread
	{
		[](TaskManager *task)
		{
			while (!task->exitFlag)
			{
				tCrsRet crs_ret;
				while (task->m_CrsRetList.get(crs_ret))
				{
					task->m_CrsRetList.get(crs_ret);
					task->AddTaskLog(console->info("[CRS] 处理开始 img_label={}", crs_ret.img_label));

					auto& result = *crs_ret.pGRpcRet;
					//结果整合
					task->m_pComposeResult->ComposeResults(result);
					//发送event事件信息
					task->AddTaskLog(console->info("[CRS] 处理结束 img_label={}", crs_ret.img_label));
					stringstream ss;
					ss << "CRS_DEAL " << crs_ret.img_label;
					//发送event事件信息
					task->SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_CRS_RET, ss.str(),
						task->m_plcInfoState.GetCurPLCInfo().gantryValue,
						task->m_plcInfoState.GetCurPLCInfo().hoistValue,
						task->m_plcInfoState.GetCurPLCInfo().trollyValue);

					//识别结果显示到图片上，保存到本地和上传FTP
					g_pPicSave->ProcessPic(result);

					for (auto &a : result.result())
					{
						if (a.rtype() == GRpcResult_RESULT_TYPE_T_CNR)
						{
							task->m_nUpdateResult++;
							break;
						}
					}

					g_mtxPicItem.lock();
					int nItemSize = g_arrPicItem.size();
					g_mtxPicItem.unlock();
					AddMainLog(console->info("处理识别结果 g_arrPicItem={}", nItemSize));
				}
				SleepMilliseconds(1);
			}
		}, this
	};
	thImgDeal.detach();

    return;
}

void TaskManager::SetTaskID()
{
	m_task_id = g_AppConfig.userCfg.quayNo + "_" + m_lane_id + "_" + GetSysTimeString();
	m_TaskInfo.set_work_id(m_task_id);
	m_TaskInfo.set_dev_id(g_AppConfig.userCfg.quayNo);
    console->debug("任务号：{}", m_task_id);
}

void TaskManager::Stop()
{
    exitFlag = true;

    worker.detach();

    listener.detach();

    console->info("任务管理模块退出");
}

string TaskManager::ShowEvents(bool b)
{
	string eveStr;
	for (const auto &a : m_eventSet)
	{
		eveStr += to_string(a);
		eveStr += ",";
	}
	if (b)
	{
		console->info("车道[{}]已完成{}/{}  [{}]", m_lane_id, m_eventSet.size(), m_TaskInfo.work_status() == 3 ? 8 : 10, eveStr);
	}
	return eveStr;
}

bool TaskManager::IsHaveEvent(int e)
{
	bool r = false;
	for (const auto &a : m_eventSet)
	{
		if (a == e)
		{
			r = true;
			break;
		}
	}
	return r;
}
bool TaskManager::InitTaskData()
{
	m_bIsSendingResult = false;
	m_planTruckMatch = false;
	m_planCnMatch = false;
	m_planTruckNum.clear();
	m_nLockType = LOCK_NONE;
	m_spreader = "";
	//将img_label控制在3000以内
	g_imgLabelMutex.lock();
	if (g_picNum > 3000)
		g_picNum = 0;
	g_imgLabelMutex.unlock();
	//清空车牌结果
	m_arrPlateHead.clear();
	m_arrPlateTail.clear();
	m_PlateResult.sLicPlate = "";
	//将接收到的箱号识别结果清空
	m_mtxCnItem.lock();
	m_arrCnItem.clear();
	m_mtxCnItem.unlock();

	m_DRisFri = 0;
	m_DRisFriAxis = 0;
	m_IARSPmn = false;
	m_CRSPmn = false;
	m_TruckNumResultNeed = false;
	m_TruckResultSend = false;
	m_CNResultNeed = false;
	m_CNResultSend = false;
	m_StartSendPmnTime = GetCurrentStamp64();
	m_IARSInnerPmn.Clear();
	m_IARSOuterPmn.Clear();
	m_CRSInnerPmn.Clear();
	m_CRSOuterPmn.Clear();
	m_truckNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	m_cnNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	m_truckType = PLCInfo::TRUCK_TYPE_INVALID;
	m_spreaderMode = PLCInfo::SPREADER_INVALID;

	m_bIsSendingResult = false;

	Sany_Code.truck_code = Truck_Code::Normal_FromPmn;
	Sany_Code.cn_code = CN_Code::ContainerNo_Normal;
	Sany_Code.door_code = DoorDir_Normal;
	Sany_Code.open_code = DoorOpen_Normal;

	m_TruckNoPlateNum.plate_num = "";
	m_TruckNoPlateNum.truck_num = "";
	m_TruckNoPlateNum.truck_num_conf = 0.0;
	m_TruckNoPlateNum.type = "";
	m_nUpdateResult = 0;
	memset(m_pmnWindowInLane, 0, sizeof(m_pmnWindowInLane));
	memset(m_hasTruckZeroFrames, 0, sizeof(m_hasTruckZeroFrames));
	m_gantryAnchor = -9999;
	for (int i = 0; i < 3; ++i)
	{
		m_pendingPlate[i].Clear();
	}
	g_mtxPicItem.lock();
	vector<tPicItem>::iterator iter;
	for (iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
	{
		//数据已经处理，删除所有处理完成的图片，主要为车牌相机图片。
		if (iter->crs_deal)
		{
			AddTaskLog(console->info("InitTaskData:数据处理完成: task_id={} img_label={} ori_name={} ", iter->task_id, iter->img_label, iter->ori_name));
			iter = g_arrPicItem.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	AddTaskLog(console->info("InitTaskData:数据处理完成:  g_arrPicItem_size={}",  g_arrPicItem.size()), true);
	g_mtxPicItem.unlock();

	std::lock_guard<std::mutex> lck(m_mtxTaskInfo);
	m_TaskInfo.Clear();
	m_TaskInfo.set_truck_head_dir("Unknow");
	m_TaskInfo.set_work_status(WS_UNFINISH);
	m_TaskInfo.set_work_type(WorkTypeUnknow);
	m_tRetInfo.Clear();
	return true;
}
bool TaskManager::UnInitTaskData()
{
	m_bIsSendingResult = false;
	m_planTruckMatch = false;
	m_planCnMatch = false;
	m_nLockType = LOCK_NONE;
	m_spreader = "";
	//将img_label控制在3000内
	g_imgLabelMutex.lock();
	if (g_picNum > 3000)
		g_picNum = 0;
	g_imgLabelMutex.unlock();
	//清空车牌结果
	m_arrPlateHead.clear();
	m_arrPlateTail.clear();
	m_PlateResult.sLicPlate = "";
	//将接收到的箱号识别结果清空
	m_mtxCnItem.lock();
	m_arrCnItem.clear();
	m_mtxCnItem.unlock();

	m_IARSPmn = false;
	m_CRSPmn = false;
	m_TruckNumResultNeed = false;
	m_TruckResultSend = false;
	m_CNResultNeed = false;
	m_CNResultSend = false;
	m_StartSendPmnTime = GetCurrentStamp64();
	m_IARSInnerPmn.Clear();
	m_IARSOuterPmn.Clear();
	m_CRSInnerPmn.Clear();
	m_CRSOuterPmn.Clear();
	m_truckNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	m_cnNumSuccess = PLCInfo::RESULT_TYPE_INVALID;
	m_truckType = PLCInfo::TRUCK_TYPE_INVALID;
	m_spreaderMode = PLCInfo::SPREADER_INVALID;

	m_bIsSendingResult = false;


	m_TruckNoPlateNum.plate_num = "";
	m_TruckNoPlateNum.truck_num = "";
	m_TruckNoPlateNum.truck_num_conf = 0.0;
	m_TruckNoPlateNum.type = "";
	m_nUpdateResult = 0;
	g_mtxPicItem.lock();
	vector<tPicItem>::iterator iter;
	for (iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
	{
		//数据已经处理，删除所有处理完成的图片，主要为车牌相机图片。
		if (iter->crs_deal)
		{
			AddTaskLog(console->info("UnInitTaskData:数据处理完成: task_id={} img_label={} ori_name={}", iter->task_id, iter->img_label, iter->ori_name));
			iter = g_arrPicItem.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	AddTaskLog(console->info("UnInitTaskData:数据处理完成: g_arrPicItem_size={}", g_arrPicItem.size()), true);
	g_mtxPicItem.unlock();

	std::lock_guard<std::mutex> lck(m_mtxTaskInfo);
	m_TaskInfo.Clear();
	m_TaskInfo.set_truck_head_dir("Unknow");
	m_TaskInfo.set_work_status(WS_FINISH);
	m_TaskInfo.set_work_type(WorkTypeUnknow);
	m_tRetInfo.Clear();
	return true;
}
bool TaskManager::IsLaneIn()
{
	for (auto &t : m_LaneCfg.m)
	{
		if (m_plcInfoState.GetCurPLCInfo().trollyValue > t.second.minValue
			&& m_plcInfoState.GetCurPLCInfo().trollyValue < t.second.maxValue)
		{
			return true;
		}
	}
	return false;
}
bool TaskManager::IsContinueTaskEnd(const int &m_eventType)
{
	if (IsHaveEvent(m_eventType)) //事件列表有已发生事件，则结束
	{
		console->info("[{}]事件已发生，异常重启作业[{}]完成", m_eventType, m_task_id);
		return true;
	}
	else
	{
		//相隔时间大于2分钟，认为是新的作业
		int64_t cur = 0;
		GetCurrentStamp64(cur);
		if (m_isContinueTask)
		{
			if (cur - taskPreContinueTime > 120000)
			{
				console->info("[{}]事件未发生，相隔时间大于2分钟，异常重启作业[{}]完成", m_eventType, m_task_id);
				return true;
			}
		}


	}
	return false;
}

void TaskManager::DealEventTruckReady()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_TRUCK_READY] 事件",m_lane_id));
	//开始保存Plc
	//提前保存一帧数据
	if (!m_isSavePlcInfo)
	{
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, m_plcInfoState.GetCurPLCInfo());
		m_isSavePlcInfo = true;
		console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
	}
}
void TaskManager::DealEventTruckLeave(int lane)
{
	AddTaskLog(console->info("[pmn] 车道[{}]触发 [EventTruckLeave] 事件， 清空车号结果", lane));
	for (auto&ret: m_arrPmnItem[lane])
	{
		AddTaskLog(console->info("[pmn] 车道[{}] 清理车号结果 {}", lane, ret.to_string()));
	}
	AddTaskLog(console->info("[pmn] 车道[{}] 清理车牌结果 {}", lane, m_arrPmnItem2[lane].to_string()));
	m_arrPmnItem[lane].clear();
	m_arrPmnItem2[lane].Clear();

	m_TaskInfo.mutable_ocr()->clear_pmr_arr();
	/*AddTaskLog(console->info(
		"车道[{}]触发 [EVENT_TRUCK_LEAVE] 事件,清理对应车道车牌数据:车顶号size:[{}][{}]车牌号size:[{}][{}],车号池size[{}]",
		lane, m_arrPmnItem[1].pmn, m_arrPmnItem[2].pmn, m_arrPmnItem2[1].pmn, m_arrPmnItem2[2].pmn, m_TaskInfo.ocr().pmr_arr().size()));*/
	//开始保存Plc
	//提前保存一帧数据
	if (!m_isSavePlcInfo)
	{
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, m_plcInfoState.GetCurPLCInfo());
		m_isSavePlcInfo = true;
		console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
	}
}
void TaskManager::DealEventLand()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_LAND] 事件", m_lane_id));
	//开始保存Plc
	//提前保存一帧数据
	if (!m_isSavePlcInfo)
	{
		PLCInfo plcTmp = m_plcInfoState.GetCurPLCInfo();
		plcTmp.catchStatus = PLCInfo::NON_CATCHING;//前一帧 非着箱
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, plcTmp);
		m_isSavePlcInfo = true;
		console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
	}

	//车道内着箱 这个事件不分装船和卸船，都会在车道内和车道外发生着箱
	if (IsLaneIn())
	{
		m_eventType = EventInfo_EVENT_TYPE_EVENT_LAND;
	}
	else
	{
		//车道外着箱
		m_eventType = EventInfo_EVENT_TYPE_EVENT_LAND + 10;
	}
	if (m_isContinueTask && IsContinueTaskEnd(m_eventType))//是异常重启作业
	{
		m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_END;
	}
	else
	{
		m_eventSet.insert(m_eventType);
		ShowEvents();
		SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_LAND, "EVENT_LAND",
			m_plcInfoState.GetCurPLCInfo().gantryValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue);
	}

	m_isContinueTask = false;
}
void TaskManager::DealEventUnLand()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_UNLAND] 事件",m_lane_id));
	//开始保存Plc
	//提前保存一帧数据
	if (!m_isSavePlcInfo)
	{
		PLCInfo plcTmp = m_plcInfoState.GetCurPLCInfo();
		plcTmp.catchStatus = PLCInfo::CATCHING;//前一帧 非着箱
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, plcTmp);
		m_isSavePlcInfo = true;
		console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
	}
}
void TaskManager::DealEventLock()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_LOCK] 事件", m_lane_id));
	//开闭锁在相同位置为无效
	if (abs(m_plcInfoState.GetCurPLCInfo().gantryValue - m_lockGantryValue) > 100
		|| abs(m_plcInfoState.GetCurPLCInfo().trollyValue - m_lockTrallyValue) > 100
		|| abs(m_plcInfoState.GetCurPLCInfo().hoistValue - m_lockHoistValue) > 100)
	{
		AddTaskLog(console->info("车道[{}]有效 [EVENT_LOCK] 事件 编码器值不同：Gantry[{},{}] Trally[{},{}] Hoist[{},{}]",
			m_lane_id,
			m_plcInfoState.GetCurPLCInfo().gantryValue, m_lockGantryValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue, m_lockTrallyValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue, m_lockHoistValue));
		//开始保存Plc
		//提前保存一帧数据
		if (!m_isSavePlcInfo)
		{
			PLCInfo plcTmp = m_plcInfoState.GetCurPLCInfo();
			plcTmp.lockStatus = PLCInfo::UNLOCKED;//前一帧 开锁
			CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, plcTmp);
			m_isSavePlcInfo = true;
			console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
		}

		if (IsLaneIn())
		{
			//车道内闭锁，收箱作业
			m_eventType = EventInfo_EVENT_TYPE_EVENT_LOCK;
			m_nLockType = LOCK_IN_LANE;
			//m_TaskInfo.set_work_type(TruckToYard);
		}
		else
		{
			//车道外（堆场）闭锁，发箱作业
			//m_TaskInfo.set_work_type(YardToTruck);
			m_eventType = EventInfo_EVENT_TYPE_EVENT_LOCK + 10;
			m_nLockType = LOCK_OUT_LANE;
		}
		if (m_isContinueTask && IsContinueTaskEnd(m_eventType))//是异常重启作业
		{
			m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_END;
		}
		else
		{
			m_eventSet.insert(m_eventType);
			ShowEvents();
			SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_LOCK, "EVENT_LOCK",
				m_plcInfoState.GetCurPLCInfo().gantryValue,
				m_plcInfoState.GetCurPLCInfo().hoistValue,
				m_plcInfoState.GetCurPLCInfo().trollyValue,
				1);
		}
		m_isContinueTask = false;
		m_lockGantryValue = m_plcInfoState.GetCurPLCInfo().gantryValue;
		m_lockTrallyValue = m_plcInfoState.GetCurPLCInfo().trollyValue;
		m_lockHoistValue = m_plcInfoState.GetCurPLCInfo().hoistValue;
		g_nLockTrallyValue = m_lockTrallyValue;
		g_nLockHoistValue = m_lockHoistValue;
		//g_nLockHoistValue1 = m_lockHoistValue;
	}
	else
	{
		AddTaskLog(console->warn("车道[{}]无效 [EVENT_LOCK] 事件 编码器值相同：Gantry[{},{}] Trally[{},{}] Hoist[{},{}]",
			m_lane_id,
			m_plcInfoState.GetCurPLCInfo().gantryValue, m_lockGantryValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue, m_lockTrallyValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue, m_lockHoistValue));

	}
	std::lock_guard<std::mutex> lock(g_mtxPicItem);
	vector<tPicItem>::iterator iter;
	std::set<std::string> set_taskId;
	for (iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
	{

		set_taskId.insert(iter->task_id);
		++iter;
	}
	for (const auto & id : set_taskId)
	{
		AddTaskLog(console->info("g_arrPicTtem中taskid={}",id));
	}
	while(set_taskId.size() > 1)//循环判断缓存数据任务数，大于2则清理过期任务数据
	{
		for (iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
		{
			//处理过期任务，删除
			if (iter->task_id == *set_taskId.begin())
			{
				iter = g_arrPicItem.erase(iter);
			}
			else
			{
				++iter;
			}
		}
		AddTaskLog(console->info("过期作业数据处理: task_id={} 删除后 g_arrPicItem.size={}",
					*set_taskId.begin(),g_arrPicItem.size()), true);
		set_taskId.erase(set_taskId.begin());//删除最小即最旧任务元素
	}
}
void TaskManager::DealEventUnLock()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_UNLOCK] 事件", m_lane_id));
	//相同位置开锁闭锁无效，滤除相同位置进行了开->闭 闭->开
	/*if (abs(m_plcInfoState.GetCurPLCInfo().gantryValue - m_lockGantryValue) > 100
		|| abs(m_plcInfoState.GetCurPLCInfo().trollyValue - m_lockTrallyValue) > 100
		|| abs(m_plcInfoState.GetCurPLCInfo().hoistValue - m_lockHoistValue) > 100)
	{*/
		AddTaskLog(console->info("车道[{}]有效 [EVENT_UNLOCK] 事件 编码器值不同：Gantry[{},{}] Trally[{},{}] Hoist[{},{}]",
			m_lane_id,
			m_plcInfoState.GetCurPLCInfo().gantryValue, m_lockGantryValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue, m_lockTrallyValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue, m_lockHoistValue));
		//开始保存Plc
		//提前保存一帧数据
		if (!m_isSavePlcInfo)
		{
			PLCInfo plcTmp = m_plcInfoState.GetCurPLCInfo();
			plcTmp.lockStatus = PLCInfo::LOCKED;//前一帧 闭锁
			CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, plcTmp);
			m_isSavePlcInfo = true;
			console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
		}

		bool isLaneIn = IsLaneIn();
		if (isLaneIn)
		{
			m_eventType = EventInfo_EVENT_TYPE_EVENT_UNLOCK + 10;
		}
		else
		{
			m_eventType = EventInfo_EVENT_TYPE_EVENT_UNLOCK;
		}
		if (m_isContinueTask && IsContinueTaskEnd(m_eventType))//是异常重启作业
		{
			m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_END;
		}
		else
		{
			m_eventSet.insert(m_eventType);
			ShowEvents();
			SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_UNLOCK, "EVENT_UNLOCK",
				m_plcInfoState.GetCurPLCInfo().gantryValue,
				m_plcInfoState.GetCurPLCInfo().hoistValue,
				m_plcInfoState.GetCurPLCInfo().trollyValue,
				isLaneIn ? 1 : 0,
				1);
		}
		m_isContinueTask = false;
		m_lockGantryValue = m_plcInfoState.GetCurPLCInfo().gantryValue;
		m_lockTrallyValue = m_plcInfoState.GetCurPLCInfo().trollyValue;
		m_lockHoistValue = m_plcInfoState.GetCurPLCInfo().hoistValue;

		//开锁后认为作业结束
		/*if (m_bRecvJobTrigger)
		{
			m_bRecvJobTrigger = false;
			m_taskState = TASKEND;
			string strInfo = console->warn("车道[{}]发生开锁，作业结束", m_lane_id);
			AddTaskLog(strInfo);
			AddMainLog(strInfo);
		}
		else
		{
			string strInfo = console->warn("车道[{}]发生开锁，未接收到JobTrigger作业[{}]不结束", m_lane_id, m_task_id);
			AddTaskLog(strInfo);
			AddMainLog(strInfo);
		}*/
	/*}
	else
	{
		AddTaskLog(console->warn("车道[{}]无效 [EVENT_UNLOCK] 事件 编码器值相同：Gantry[{},{}] Trally[{},{}] Hoist[{},{}]",
			m_lane_id,
			m_plcInfoState.GetCurPLCInfo().gantryValue, m_lockGantryValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue, m_lockTrallyValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue, m_lockHoistValue));
	}*/
}
void TaskManager::DealEventTaskEnd()
{
	AddTaskLog(console->info("车道[{}]触发 [EVENT_TASK_END] 事件", m_lane_id));
	AddTaskLog(console->info("车道[{}]有效 [EVENT_TASK_END] 事件 编码器值不同：Gantry[{},{}] Trally[{},{}] Hoist[{},{}]",
		m_lane_id,
		m_plcInfoState.GetCurPLCInfo().gantryValue, m_lockGantryValue,
		m_plcInfoState.GetCurPLCInfo().trollyValue, m_lockTrallyValue,
		m_plcInfoState.GetCurPLCInfo().hoistValue, m_lockHoistValue));
	//开始保存Plc
	//提前保存一帧数据
	if (!m_isSavePlcInfo)
	{
		PLCInfo plcTmp = m_plcInfoState.GetCurPLCInfo();
		plcTmp.taskState = PLCInfo::TASK_STATE_END;
		CRecodData::GetInstance()->AddPlcInfo(m_TaskInfo.local_snapshot_location() + m_task_id, plcTmp);
		m_isSavePlcInfo = true;
		console->info("[{}]开始保存本次作业的PLC", m_TaskInfo.work_id());
	}

	bool isLaneIn = IsLaneIn();
	if (isLaneIn)
	{
		m_eventType = EventInfo_EVENT_TYPE_EVENT_UNLOCK + 10;
	}
	else
	{
		m_eventType = EventInfo_EVENT_TYPE_EVENT_UNLOCK;
	}
	if (m_isContinueTask && IsContinueTaskEnd(m_eventType))//是异常重启作业
	{
		m_eventType = EventInfo_EVENT_TYPE_EVENT_TASK_END;
	}
	else
	{
		m_eventSet.insert(m_eventType);
		ShowEvents();
		SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_UNLOCK, "EVENT_UNLOCK",
			m_plcInfoState.GetCurPLCInfo().gantryValue,
			m_plcInfoState.GetCurPLCInfo().hoistValue,
			m_plcInfoState.GetCurPLCInfo().trollyValue,
			isLaneIn ? 1 : 0,
			1);
	}
	m_isContinueTask = false;
	m_lockGantryValue = m_plcInfoState.GetCurPLCInfo().gantryValue;
	m_lockTrallyValue = m_plcInfoState.GetCurPLCInfo().trollyValue;
	m_lockHoistValue = m_plcInfoState.GetCurPLCInfo().hoistValue;
}
//发送事件信息
void TaskManager::SendTaskInfo(EventInfo::EVENT_TYPE type, std::string event_name, int gantry_pos, int hoist_pos, int trolly_pos, int isLaneIn, int needReply)
{
	OCRAny any;
	any.set_msg_id(GetUUID());
	any.set_msg_type(MSG_TYPE::MSG_EVENTINFO);
	any.set_src_id(g_AppConfig.userCfg.IARSName);
	any.set_need_reply(needReply);
	any.set_time(GetTimeString());
	any.set_dev_id(g_AppConfig.userCfg.quayNo);

	EventInfo eventInfo;
	m_mtxTaskInfo.lock();
	eventInfo.set_work_id(m_TaskInfo.work_id());
	eventInfo.set_type(type);
	//eventInfo.set_trigger("");
	eventInfo.set_desc(event_name);
	eventInfo.set_work_type(m_TaskInfo.work_type());
	eventInfo.set_spreader(m_TaskInfo.spreader());
	if (m_TaskInfo.ocr().pmr_arr_size() > 0)
	{
		string pmn;
		float fConfidence = 0;
		for (auto& t: m_TaskInfo.ocr().pmr_arr())
		{
			if (t.pmr().rect_pmn().confidence() > fConfidence)
			{
				fConfidence = t.pmr().rect_pmn().confidence();
				pmn = t.pmr().pmn();
			}
		}
		eventInfo.set_truck_plate(pmn);//车号
	}
	else if (m_PlateResult.sLicPlate.length())
	{
		eventInfo.set_truck_plate(m_PlateResult.sLicPlate);//车牌号或车架号
	}
	else //没有车号设置????
	{
		eventInfo.set_truck_plate("????");//车号
	}
	m_mtxTaskInfo.unlock();
	//不为默认值才赋值
	if (isLaneIn != 2)
	{
		eventInfo.set_is_lane_in(isLaneIn);
	}
	DataPlc *plcInfo = eventInfo.mutable_data_plc();
	plcInfo->set_gantry_pos(gantry_pos);
	plcInfo->set_hoist_pos(hoist_pos);
	plcInfo->set_trolly_pos(trolly_pos);

	any.mutable_any()->PackFrom(eventInfo);
	//EventInfo当前未使用，为较少服务器压力，这个消息不再发送。yeming-20230303
	//g_WaitPubMsg.add(any);

	AddTaskLog(ConvertChs(eventInfo.DebugString()));
}

std::map<string, TaskManager::PmnSet> TaskManager::AggregatePmnInWindow(
	const std::vector<PmnResult>& samples,
	int reqGantry,
	int gantryDelta,
	int* outInWindow)
{
	std::map<string, PmnSet> winSet;
	int inWindow = 0;
	for (const auto& ret : samples)
	{
		// |样本gantry - 请求时gantry|≤delta 才计票
		if (abs(ret.GantryValue - reqGantry) <= gantryDelta)
		{
			++inWindow;
			PmnSet& s = winSet[ret.pmn];
			s.conf += ret.conf;
			s.nCount += 1;
			s.lastGantry = ret.GantryValue;
			if (!ret.path.empty())
				s.path = ret.path;
			if (ret.fromQr)
				s.fromQr = true;
		}
	}
	if (outInWindow)
		*outInWindow = inWindow;
	return winSet;
}

bool TaskManager::IsPlateInSendWindow(const PmnResult& plate, int reqGantry, int gantryDelta)
{
	if (!g_AppConfig.userCfg.pmnWindowEnable)
	{
		return true;
	}
	const int d = plate.GantryValue - reqGantry;
	// 允许：停稳抖动约 -100～0、同位 0、前冲 0～delta；
	// 说明：大车从编码器值小向编码器值大移动时忽略识别结果，防止上传过路车。
	// 正常情况下大车从编码器值小向编码器值大移动时，大车会向前移动一定距离，
	// 针对两车排队时，没有识别到大车正下方的前车，而上传了路过的后车的车牌。
	return d >= -100 && d <= gantryDelta;
}

bool TaskManager::SelectBestPmn(
	const std::map<string, PmnSet>& winSet,
	string& outPmn,
	PmnSet& outSet)
{
	outPmn.clear();
	int bestCount = 0;
	float bestConf = 0.f;
	for (const auto& kv : winSet)
	{
		const PmnSet& s = kv.second;
		if (s.nCount > bestCount ||
			(s.nCount == bestCount && s.conf > bestConf))
		{
			bestCount = s.nCount;
			bestConf = s.conf;
			outPmn = kv.first;
			outSet = s;
		}
	}
	return !outPmn.empty();
}

void TaskManager::FillPmnResultFromSet(
	const string& pmn,
	const PmnSet& set,
	PmnResult& out)
{
	out.Clear();
	out.pmn = pmn;
	out.conf = set.conf;
	out.nCount = set.nCount;
	out.GantryValue = set.lastGantry;
	out.path = set.path;
	out.fromQr = set.fromQr;
	out.type = PLCInfo::TRUCK_TYPE_INNER;
}

// PLC 拉车顶号/车牌：按 PIIS 请求瞬间的 gantry 做发送窗口（不要求停稳）；窗外置空，缓存保留。
void TaskManager::GetPmnResult(string& json_str)
{
	m_plcinfoMtx.lock();
	PLCInfo plc = m_plcinfo;
	m_plcinfoMtx.unlock();

	cJSON *root = cJSON_CreateObject();
	cJSON_AddTrueToObject(root, "IsSuccess");
	cJSON_AddStringToObject(root, "Message", "OK");

	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "Data", data);
	cJSON_AddStringToObject(data, "UUID", GetUUID().c_str());
	cJSON_AddStringToObject(data, "Timestamp", GetHttpTimeString().c_str());
	cJSON_AddStringToObject(data, "TaskId", plc.taskId.c_str());
	cJSON_AddStringToObject(data, "CraneId", plc.craneId.c_str());

	console->info("作业车道[{}]，有车车道[{}][{}]", plc.workLine, plc.hasTruck[1], plc.hasTruck[2]);
	// 打印日志，说明作业车道和有车情况
	if (plc.workLine < 1)
	{
		AddTaskLog(console->info("[pmn][send] 作业车道无效[{}]，不上传车顶号和车牌", plc.workLine));
		plc.workLine = -1;
	}
	// 车道1，2无车
	else if ((plc.workLine == 1 || plc.workLine == 2) && plc.hasTruck[plc.workLine] != 1)
	{
		const int otherLane = 3 - plc.workLine;
		if (plc.hasTruck[otherLane] == 1)
		{
			AddTaskLog(console->info("[pmn][send] 作业车道[{}]无车，车道[{}]有车",
				plc.workLine, otherLane));
		}
	}
	cJSON_AddNumberToObject(data, "LandNo", plc.workLine);

	string imgPath;
	g_mtxPicItem.lock();
	for (auto&item : g_arrPicItem)
	{
		for (auto&param : item.arrParam)
		{
			if (param.param.type() == ocrs::pb::GRpc_RecogParam_G_RType_PMN)
			{
				imgPath = g_AppConfig.userCfg.imgRootPath + "\\" + m_task_dir + "\\" + item.ori_img_name;
				break;
			}
		}
	}
	g_mtxPicItem.unlock();

	bool bIsHavePmnResult = false;
	// 每次 GET 都算：窗口中心=本次请求时的 plc.gantryValue，不要求大车停稳；窗外不算分，窗内无票回空
	PmnResult winRet;
	winRet.Clear();
	PmnResult *pPmnRet = nullptr;
	m_mtxPmnItem.lock();
	const int reqGantry = plc.gantryValue; // PIIS 请求瞬间的大车编码器，车顶号/车牌发送窗共用
	const int gantryDelta = g_AppConfig.userCfg.pmnWindowGantryDelta;
	// 上传：实时 hasTruck==1，或门控迟滞仍在窗内（离场消抖中 hasTruck 短暂为 0 仍可发）
	const bool workLaneHasTruck =
		(plc.workLine >= 1 && plc.workLine <= 2 && plc.hasTruck[plc.workLine] == 1);
	const bool workLaneInWindow =
		(plc.workLine >= 1 && plc.workLine <= 2 && m_pmnWindowInLane[plc.workLine]);
	const bool workLaneCanSend = workLaneHasTruck || workLaneInWindow;
	// 有车时补提交早到的暂存车牌（已持 m_mtxPmnItem；与 UpdatePmnWindowState 互补）
	if (workLaneHasTruck)
	{
		CommitPendingPlateUnderLock(plc.workLine);
	}
	auto iter = m_arrPmnItem.find(plc.workLine);

	// 可发送且车道内有样本时才计算
	if (!workLaneCanSend)
	{
		AddTaskLog(console->info("[pmn][send] 作业车道不可发送，PlateTop/TruckPlate 置空 workLine={} hasTruck1={} hasTruck2={} inWindow1={} inWindow2={}",
			plc.workLine, plc.hasTruck[1], plc.hasTruck[2],
			m_pmnWindowInLane[1] ? 1 : 0, m_pmnWindowInLane[2] ? 1 : 0));
	}
	else if (iter != m_arrPmnItem.end() && !iter->second.empty())
	{
		auto& arrRet = iter->second;

		int inWindow = 0;
		auto winSet = AggregatePmnInWindow(arrRet, reqGantry, gantryDelta, &inWindow);

		// 诊断日志：窗内各号票数/置信度，便于核对排队车是否被窗口滤掉
		string tally;
		for (auto& kv : winSet)
		{
			tally += fmt::format("{} nCount={} conf={:.3f}; ", kv.first, kv.second.nCount, kv.second.conf);
		}
		AddTaskLog(console->info("[pmn][send] 窗内聚合 lane={} reqGantry={} delta={} poolSize={} inWindow={} [{}]",
			plc.workLine, reqGantry, gantryDelta, (int)arrRet.size(), inWindow,
			tally.empty() ? "(empty)" : tally));

		string bestPmn;
		PmnSet bestSet;
		if (SelectBestPmn(winSet, bestPmn, bestSet))
		{
			FillPmnResultFromSet(bestPmn, bestSet, winRet);
			pPmnRet = &winRet;
			bIsHavePmnResult = true;
			AddTaskLog(console->info("[pmn][send] 窗内选中 pmn={} nCount={} conf={} voteGantry={} reqGantry={} delta={}",
				pPmnRet->pmn, pPmnRet->nCount, pPmnRet->conf, pPmnRet->GantryValue, reqGantry, gantryDelta));
		}
		else
		{
			// 池里可能有路过样本，但都不在本次请求位置窗口 → 回空让 PIIS 继续拉
			AddTaskLog(console->info("[pmn][send] 窗内无样本，PlateTop 置空 workLine={} reqGantry={} delta={} poolSize={}",
				plc.workLine, reqGantry, gantryDelta, (int)arrRet.size()));
		}
	}
	else
	{
		AddTaskLog(console->warn("[pmn] 车道[{}] 无车号样本", plc.workLine));
		AddTaskLog(console->info("[pmn][send] 无有效票，PlateTop 将置空 workLine={}", plc.workLine));
	}
	for (auto &item : m_arrPmnItem2)
	{
		if (!item.second.pmn.empty())
		{
			bIsHavePmnResult = true;
		}
		console->info("[pmn] 车牌数据预览 lane[{}]  truck[{}]", item.first, item.second.pmn);
	}

	if (m_reqPmnCount == 1 &&
		!m_planTruckMatch) //每次作业第一次请求不处理，没有匹配的识别结果，之后的每次请求1次触发一次拍照
	{
		for (auto &gun : m_pDevMgr->gunDevList)
		{
			gun.second->m_getPmnResult = true;
		}
	}
	else
	{
		m_reqPmnCount++;
	}

	// ---------- StatusCode（Truck_Code）判码 ----------
	// 约定
	//   0 正常（内：车顶号一致；外：车牌一致）；1 正常-二维码；2 CRS发图超时；3 CRS调用失败（非超时）；
	//   4 车顶号不一致或空结果（含无车）；5 车顶号置信度低；
	//   6 无牌车（有车头，无车牌）；7 车牌识别错误或为空（无车头，无车牌）。
	// 内集卡：有车顶号→0/1/4/5，可覆盖 2/3；无号时若已是 2/3 则保持，否则写 4（含车道无车）。
	// 外集卡：只判车牌→0/6/7；无牌车→6；错识/空结果→7（含车道无车，不保持默认 0）。
	const int truckType = plc.truckType; // 用本次辅助数据中的类型，避免仅靠 m_truckType 滞后
	const bool isOuterTruck = (truckType == PLCInfo::TRUCK_TYPE_OUTER);
	const bool isCrsTimeoutCode =
		Sany_Code.truck_code == TopCode_TimeoutOnline ||
		Sany_Code.truck_code == TopCode_TimeoutOffline; // 2/3：无车顶号时保留；有号时允许覆盖

	std::string truckPlateNum; // 去掉省字头后的车牌，仅外集卡 StatusCode 比对用
	bool hasPlateResult = false; // 本车道是否有车牌缓存
	bool isNoPlateCar = false;   // 识别结果为「无牌车」→ StatusCode 6
	string strPlateTop;    // 与 JSON PlateTop 一致，同步 g_OCRToSANY.strTruckNo
	string strTruckPlate;  // 与 JSON TruckPlate 一致，同步 g_OCRToSANY.strPlate

	// ------ 组包 PlateTop -------
	if (pPmnRet)
	{
		AddTaskLog(console->warn("[pmn] 车道[{}] 使用车号结果 {}", plc.workLine, pPmnRet->to_string()));
		GetTruckNumFromMap(pPmnRet->pmn);
		strPlateTop = pPmnRet->pmn;

		cJSON_AddBoolToObject(data, "PlateTopSuccess", true);
		cJSON_AddStringToObject(data, "PlateTop", strPlateTop.c_str());
		cJSON_AddStringToObject(data, "PlateTopPic", pPmnRet->path.c_str());
	}
	else
	{
		console->info("[pmn] 当前车道没有车顶号，组包空结果，车道号[{}]", plc.workLine);
		cJSON_AddBoolToObject(data, "PlateTopSuccess", true);
		cJSON_AddStringToObject(data, "PlateTop", "");
		cJSON_AddStringToObject(data, "PlateTopPic", "");
	}

	// -------- 组包 TruckPlate：可发送才上传；再按 reqGantry 窗口过滤，窗外置空但不清缓存 --------
	const PmnResult* pPlateCache = nullptr;
	bool plateInWindow = true;
	auto plateIter = m_arrPmnItem2.find(plc.workLine);
	if (!workLaneCanSend)
	{
		if (plateIter != m_arrPmnItem2.end() && !plateIter->second.pmn.empty())
		{
			AddTaskLog(console->info("[pmn][send] 作业车道不可发送，不上传车牌结果 lane={} plate=[{}] hasTruck={} inWindow={}",
				plc.workLine, plateIter->second.pmn,
				workLaneHasTruck ? 1 : 0, workLaneInWindow ? 1 : 0));
		}
	}
	else if (plateIter != m_arrPmnItem2.end() && !plateIter->second.pmn.empty())
	{
		pPlateCache = &plateIter->second;
		plateInWindow = IsPlateInSendWindow(*pPlateCache, reqGantry, gantryDelta);
		if (!plateInWindow)
		{
			AddTaskLog(console->info("[pmn][send] 车牌窗外置空 lane={} plateGantry={} reqGantry={} delta={} plate=[{}]",
				plc.workLine, pPlateCache->GantryValue, reqGantry, gantryDelta, pPlateCache->pmn));
		}
	}

	if (pPlateCache && plateInWindow)
	{
		const PmnResult& plate = *pPlateCache;
		if (plate.pmn.find("无牌车") != string::npos)
		{
			hasPlateResult = true;
			isNoPlateCar = true; // 外集卡 StatusCode=6
			strTruckPlate = GbkToUtf8(truckPlateNum);
			cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
			cJSON_AddStringToObject(data, "TruckPlate", strTruckPlate.c_str());
			cJSON_AddStringToObject(data, "TruckPlatePic", plate.path.c_str());
		}
		else if (plate.pmn.length() > 2) // 含2字节汉字省字头，截取后与任务号比
		{
			hasPlateResult = true;
			console->info("车牌识别位置[{}]，当前位置[{}]", plate.GantryValue, reqGantry);
			truckPlateNum = plate.pmn.substr(2); // 去掉省字头
			strTruckPlate = GbkToUtf8(truckPlateNum);
			cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
			cJSON_AddStringToObject(data, "TruckPlate", strTruckPlate.c_str());
			cJSON_AddStringToObject(data, "TruckPlatePic", plate.path.c_str());
		}
		else
		{
			console->info("当前车道车牌结果过短,组包空结果，车道号[{}]", plc.workLine);
			cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
			cJSON_AddStringToObject(data, "TruckPlate", "");
			cJSON_AddStringToObject(data, "TruckPlatePic", plate.path.c_str());
		}
	}
	else
	{
		if (pPlateCache && !plateInWindow)
		{
			console->info("当前车道车牌在窗外,组包空结果，车道号[{}]", plc.workLine);
		}
		else
		{
			console->info("当前车道没有外集卡车号,组包空结果，车道号[{}]", plc.workLine);
		}
		cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
		cJSON_AddStringToObject(data, "TruckPlate", "");
		cJSON_AddStringToObject(data, "TruckPlatePic", "");
	}

	// -- 按集卡类型写 StatusCode（与字段组包分离）
	if (isOuterTruck)
	{
		// 【外集卡】只判车牌：0 / 6 / 7；禁止因车顶号写 4/5
		if (isNoPlateCar)
		{
			Sany_Code.truck_code = Truck_Code::Plate_NoResult; // 6：识别为无牌车
			AddTaskLog(console->info("[pmn][outer] 无牌车，TaskId {}，StatusCode {}",
				plc.taskId, Sany_Code.truck_code));
		}
		else if (hasPlateResult && !truckPlateNum.empty())
		{
			if (truckPlateNum.find(plc.truckNumStr) != string::npos)
			{
				m_planTruckMatch = true;
				Sany_Code.truck_code = Truck_Code::Normal_FromPmn; // 0：外集卡车牌与任务一致
				AddTaskLog(console->info("[pmn][outer] 车牌[{}]与任务[{}] 一致，识别正常（来自车牌），TaskId {}，StatusCode {}",
					truckPlateNum, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
			}
			else
			{
				Sany_Code.truck_code = Truck_Code::Plate_Mismatch; // 7：车牌与任务不一致
				AddTaskLog(console->info("[pmn][outer] 车牌[{}]与任务[{}] 不一致，TaskId {}，StatusCode {}",
					truckPlateNum, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
			}
		}
		else
		{
			// 空结果 / 过短 / 窗外置空 / 无缓存（含车道无车）：一律 7，不保持默认 0
			Sany_Code.truck_code = Truck_Code::Plate_Mismatch; // 7
			AddTaskLog(console->info("[pmn][outer] 车牌识别为空或无效，TaskId {}，StatusCode {}",
				plc.taskId, Sany_Code.truck_code));
		}
	}
	else
	{
		// 【内集卡等】只判车顶号：0/1/4/5；有号时覆盖先前 CRS 的 2/3（流识别可补号）
		if (pPmnRet)
		{
			if (pPmnRet->pmn == plc.truckNumStr)
			{
				m_planTruckMatch = true;
				if (pPmnRet->fromQr)
				{
					Sany_Code.truck_code = Truck_Code::Normal_FromQR; // 1：正常，结果来自二维码
					AddTaskLog(console->info("[pmn][inner] 车顶号[{}]与任务[{}] 一致，识别正常（来自二维码），TaskId {}，StatusCode {}",
						pPmnRet->pmn, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
				}
				else if (pPmnRet->nCount > 0 && pPmnRet->conf / pPmnRet->nCount < 0.8)
				{
					Sany_Code.truck_code = Truck_Code::TopCode_LowConfidence; // 5：车顶号置信度低
					AddTaskLog(console->info("[pmn][inner] 车顶号[{}]与任务[{}] 一致，置信度低，TaskId {}，StatusCode {}",
						pPmnRet->pmn, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
				}
				else
				{
					Sany_Code.truck_code = Truck_Code::Normal_FromPmn; // 0：正常，结果来自车顶号 OCR
					AddTaskLog(console->info("[pmn][inner] 车顶号[{}]与任务[{}] 一致，识别正常（来自车顶号），TaskId {}，StatusCode {}",
						pPmnRet->pmn, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
				}
			}
			else
			{
				Sany_Code.truck_code = Truck_Code::TopCode_Mismatch_NeedChangeTruck; // 4：车顶号与任务不一致
				AddTaskLog(console->info("[pmn][inner] 车顶号[{}]与任务[{}] 不一致，TaskId {}，StatusCode {}",
					pPmnRet->pmn, plc.truckNumStr, plc.taskId, Sany_Code.truck_code));
			}
		}
		else if (isCrsTimeoutCode)
		{
			// 无车顶号且 CRS 已记 2/3：保持超时/调用失败码（流识别也未补上）
			AddTaskLog(console->info("[pmn][inner] 无车顶号，保持 CRS状态码，TaskId {}，StatusCode {}",
				plc.taskId, Sany_Code.truck_code));
		}
		else
		{
			// 无车顶号空结果（含车道无车）：写 4，不保持默认 0
			Sany_Code.truck_code = Truck_Code::TopCode_Mismatch_NeedChangeTruck; // 4
			AddTaskLog(console->info("[pmn][inner] 无车顶号空结果，TaskId {}，StatusCode {}",
				plc.taskId, Sany_Code.truck_code));
		}
	}

	cJSON_AddNumberToObject(data, "StatusCode", Sany_Code.truck_code);

	//if (m_planTruckMatch)
	//{
	//	if (m_truckType == PLCInfo::TRUCK_TYPE_INNER)
	//	{
	//		//车号映射
	//		GetTruckNumFromMap(m_planTruckNum);
	//		cJSON_AddBoolToObject(data, "PlateTopSuccess", true);
	//		cJSON_AddStringToObject(data, "PlateTop", m_planTruckNum.c_str());
	//		cJSON_AddStringToObject(data, "PlateTopPic", imgPath.c_str());
	//	}
	//	else if (m_truckType == PLCInfo::TRUCK_TYPE_OUTER)
	//	{
	//		//std::string planTruckNum = m_planTruckNum.substr(2);
	//		cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
	//		cJSON_AddStringToObject(data, "TruckPlate", GbkToUtf8(m_planTruckNum).c_str());
	//		cJSON_AddStringToObject(data, "TruckPlatePic", imgPath.c_str());
	//	}
	//	else
	//	{
	//		console->info("m_truckType[{}]  m_planTruckNum[{}]", m_truckType, m_planTruckNum);
	//	}
	//}
	//else
	//{
	//	if (plc.workLine < 1)
	//	{
	//		if (m_truckType == PLCInfo::TRUCK_TYPE_INNER)
	//		{
	//			for (auto &pmnItem : m_arrPmnItem)
	//			{
	//				if (!pmnItem.second.pmn.empty())
	//				{
	//					if (pmnItem.second.type == 1)
	//					{
	//						//车号映射
	//						AddTaskLog(console->info("[pmn] pmn before map result [{}]", pmnItem.second.pmn));
	//						GetTruckNumFromMap(pmnItem.second.pmn);
	//						AddTaskLog(console->info("[pmn] pmn after map result [{}]", pmnItem.second.pmn));

	//						cJSON_AddBoolToObject(data, "PlateTopSuccess", true);
	//						cJSON_AddStringToObject(data, "PlateTop", GbkToUtf8(pmnItem.second.pmn).c_str());
	//						cJSON_AddStringToObject(data, "PlateTopPic", imgPath.c_str());
	//						break;
	//					}
	//				}
	//			}
	//		}
	//		else
	//		{
	//			for (auto &pmnItem : m_arrPmnItem2)
	//			{
	//				if (!pmnItem.second.pmn.empty())
	//				{
	//					if (pmnItem.second.type == 2)
	//					{
	//						std::string TruckNum = pmnItem.second.pmn.substr(2);
	//						cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
	//						cJSON_AddStringToObject(data, "TruckPlate", GbkToUtf8(TruckNum).c_str());
	//						cJSON_AddStringToObject(data, "TruckPlatePic", imgPath.c_str());
	//						break;
	//					}
	//				}
	//			}
	//		}
	//	}
	//	else if (m_truckType == PLCInfo::TRUCK_TYPE_INNER)
	//	{
	//		if (m_arrPmnItem[plc.workLine].type == 1)
	//		{
	//			//车号映射
	//			AddTaskLog(console->info("[pmn] pmn- before map result [{}]", m_arrPmnItem[plc.workLine].pmn));
	//			GetTruckNumFromMap(m_arrPmnItem[plc.workLine].pmn);
	//			AddTaskLog(console->info("[pmn] pmn- after map result [{}]", m_arrPmnItem[plc.workLine].pmn));

	//			cJSON_AddBoolToObject(data, "PlateTopSuccess", true);
	//			cJSON_AddStringToObject(data, "PlateTop", GbkToUtf8(m_arrPmnItem[plc.workLine].pmn).c_str());
	//			cJSON_AddStringToObject(data, "PlateTopPic", imgPath.c_str());
	//		}
	//	}
	//	else if (m_truckType == PLCInfo::TRUCK_TYPE_OUTER)
	//	{
	//		if (m_arrPmnItem2[plc.workLine].type == 2)
	//		{
	//			std::string TruckNum = m_arrPmnItem2[plc.workLine].pmn.substr(2);
	//			cJSON_AddBoolToObject(data, "TruckPlateSuccess", true);
	//			cJSON_AddStringToObject(data, "TruckPlate", GbkToUtf8(TruckNum).c_str());
	//			cJSON_AddStringToObject(data, "TruckPlatePic", imgPath.c_str());
	//		}
	//	}
	//	else
	//	{
	//		console->info("m_truckType[{}]  workLine[{}]", m_truckType, plc.workLine);
	//	}
	//}

	m_mtxPmnItem.unlock();

	char *printed = cJSON_Print(root);
	json_str = printed ? printed : "";
	cJSON_free(printed);
	cJSON_Delete(root);

	bool bUpdateRet = false;
	if (g_OCRToSANY.strTruckNo != strPlateTop)
	{
		g_OCRToSANY.strTruckNo = strPlateTop;
		bUpdateRet = true;
	}
	if (g_OCRToSANY.strPlate != strTruckPlate)
	{
		g_OCRToSANY.strPlate = strTruckPlate;
		bUpdateRet = true;
	}
	if (g_OCRToSANY.truck_code != Sany_Code.truck_code)
	{
		g_OCRToSANY.truck_code = Sany_Code.truck_code;
		bUpdateRet = true;
	}
	if (bUpdateRet)
	{
		//结果变化时保存到日志
		AddMainLog(fmt::format("发送车号结果，{}", json_str));
	}
}
void TaskManager::GetCnResult(string& json_str)
{
	m_plcinfoMtx.lock();
	PLCInfo plc = m_plcinfo;
	m_plcinfoMtx.unlock();

	tCnItem cnItem;
	AddTaskLog(console->info("[plc] http get cn result lane[{}] resultsize[{}]", plc.workLine, m_arrCnItem[plc.workLine].size()));

	if (!m_planCnMatch)
	{
		for (auto &gun : m_pDevMgr->gunDevList)
		{
			gun.second->m_getCnResult = true;
		}
	}
	if (plc.workLine < 1)
	{
		for (auto &item : m_arrCnItem)
		{
			if (!item.second.empty())
			{
				ComposeCnRet(item.second, cnItem);
			}
		}
	}
	else
	{
		if (!m_arrCnItem[plc.workLine].empty())
		{
			ComposeCnRet(m_arrCnItem[plc.workLine], cnItem);
		}
		else
		{
			AddTaskLog(console->info("[plc] http get cn result lane[3] resultsize[{}]", m_arrCnItem[3].size()));
			if (!m_arrCnItem[3].empty())
			{
				ComposeCnRet(m_arrCnItem[3], cnItem);
			}
		}
	}

	AddTaskLog(console->info("[plc] http get cn result [{}]", cnItem.cn_str));

	if (cnItem.cn_str.find("?") != string::npos)
	{
		cnItem.cn_str = "";
	}
	if (cnItem.iso.find("?") != string::npos)
	{
		cnItem.iso = "";
	}
	if (Sany_Code.cn_code != CN_Code::ContainerNo_TimeoutOffline && Sany_Code.cn_code != CN_Code::ContainerNo_TimeoutOnline)
	{
		AddTaskLog(console->info("[cn] 箱号结果[{}]与任务[{}] {}", cnItem.cn_str, plc.containerNo1, cnItem.cn_str == plc.containerNo1 ? "一致" : "不一致"));
		if (cnItem.cn_str == plc.containerNo1)
		{
			m_planCnMatch = true;
			if (cnItem.conf < 0.8)
			{
				Sany_Code.cn_code = CN_Code::ContainerNo_LowConfidence;
			}
			else if (cnItem.iso_conf< 0.8)
			{
				Sany_Code.cn_code = CN_Code::ISO_LowConfidence;
			}
			else
			{
				Sany_Code.cn_code = CN_Code::ContainerNo_Normal;
			}
		}
		else
		{
			Sany_Code.cn_code = CN_Code::ContainerNo_Mismatch ;
		}
	}


	cJSON *root = cJSON_CreateObject();
	cJSON_AddTrueToObject(root, "IsSuccess");
	cJSON_AddStringToObject(root, "Message", "OK");

	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "Data", data);
	cJSON_AddStringToObject(data, "UUID", GetUUID().c_str());
	cJSON_AddStringToObject(data, "Timestamp", GetHttpTimeString().c_str());
	cJSON_AddStringToObject(data, "TaskId", plc.taskId.c_str());
	cJSON_AddStringToObject(data, "CraneId", plc.craneId.c_str());
	cJSON_AddNumberToObject(data, "LandNo", plc.workLine);
	cJSON_AddBoolToObject(data, "IsSuccess", cnItem.cn_str.empty()?false:true);
	cJSON_AddStringToObject(data, "ContainerNo1",GbkToUtf8( cnItem.cn_str).c_str());
	cJSON_AddStringToObject(data, "ContainerNo2", "");
	cJSON_AddStringToObject(data, "ContainerType1", GbkToUtf8(cnItem.iso).c_str());
	cJSON_AddStringToObject(data, "ContainerType2", "");

	cJSON_AddNumberToObject(data, "StatusCode", Sany_Code.cn_code);
	g_mtxPicItem.lock();

	string cnPath;
	bool isFind = false;
	for (auto&item : g_arrPicItem)
	{
		for (auto&param : item.arrParam)
		{
			if (param.param.type() == ocrs::pb::GRpc_RecogParam_G_RType_CN)
			{
				cnPath = g_AppConfig.userCfg.imgRootPath + "\\" + m_task_dir + "\\" + item.ori_img_name;
				isFind = true;
				break;
			}
		}
		if (isFind)
		{
			break;
		}
	}

	g_mtxPicItem.unlock();
	cJSON_AddStringToObject(data, "ContainerNoPicture1", cnPath.c_str());

	char *printed = cJSON_Print(root);
	json_str = printed ? printed : "";
	cJSON_free(printed);
	cJSON_Delete(root);

	bool bUpdateRet = false;
	if (g_OCRToSANY.strContNo != cnItem.cn_str)
	{
		g_OCRToSANY.strContNo = cnItem.cn_str;
		bUpdateRet = true;
	}
	if(g_OCRToSANY.strContIso != cnItem.iso)
	{
		g_OCRToSANY.strContIso = cnItem.iso;
		bUpdateRet = true;
	}
	if (g_OCRToSANY.cn_code != Sany_Code.cn_code)
	{
		g_OCRToSANY.cn_code = Sany_Code.cn_code;
		bUpdateRet = true;
	}
	if (bUpdateRet)
	{
		//结果变化时保存到日志
		AddMainLog(fmt::format("发送箱号结果， {}", json_str));
	}
}

void TaskManager::ClearCnResultOnHoistToContUp()
{
	const int upVal = g_AppConfig.userCfg.hoistToContUpVal;
	AddTaskLog(console->info("吊具到箱顶距离[{}->{}]向上经过{}，清空箱号结果与箱号图片缓存",
		m_plcInfoState.GetLastPLC().hoistToCont,
		m_plcInfoState.GetCurPLCInfo().hoistToCont,
		upVal), true);

	// 1) 清空各车道箱号候选，下一轮下降 CAP_TOP 重新累计
	{
		std::lock_guard<std::mutex> lck(m_mtxCnItem);
		int nCnCount = 0;
		for (auto& LaneItem : m_arrCnItem)
		{
			nCnCount += (int)LaneItem.second.size();
		}
		m_arrCnItem.clear();
		AddTaskLog(console->info("清空箱号候选共{}条", nCnCount));
	}

	// 2) 移除当前任务中带 CN 识别的图片缓存项（不删本地文件，避免占锁删盘与 PicSave 竞态）
	int nRemoved = 0;
	g_mtxPicItem.lock();
	for (auto iter = g_arrPicItem.begin(); iter != g_arrPicItem.end(); )
	{
		bool bCnPic = false;
		// 识别参数
		for (auto& crsParam : iter->arrParam)
		{
			if (crsParam.param.type() == ocrs::pb::GRpc_RecogParam_G_RType_CN)
			{
				bCnPic = true;
				break;
			}
		}
		if (bCnPic && (iter->task_id.empty() || iter->task_id == m_task_id))
		{
		    // 清空缓存
			iter = g_arrPicItem.erase(iter);
			++nRemoved;
		}
		else
		{
			++iter;
		}
	}
	g_mtxPicItem.unlock();

	// 3) 复位计划匹配，后续 GetCnResult 可再次触发补拍
	m_planCnMatch = false;

	AddTaskLog(console->info("清空箱号完成：候选已清，移除箱号图片缓存{}项（保留本地文件），等待再次下降到{}cm重拍",
		nRemoved, upVal), true);
}

void TaskManager::ComposeCnRet(std::vector<tCnItem> cnArr,tCnItem& cn_item)
{
	m_mtxCnItem.lock();
	m_pComposeResult->ComposeCnRet(cnArr, cn_item);
	m_mtxCnItem.unlock();
}

void TaskManager::AddTaskLog(const string& info, bool bMainLog)
{
	shared_ptr<tTaskLog> pLog = std::make_shared<tTaskLog>(m_TaskInfo);
	pLog->info = info;
	m_queueLog.add(pLog);
	if (bMainLog)
		AddMainLog(info);
}

void TaskManager::SaveLogToFile()
{
	shared_ptr<tTaskLog> pLog;
	while (m_queueLog.get(pLog))
	{
		FILE *pFile = nullptr;
		fopen_s(&pFile, pLog->path.c_str(), "a+t");
		if (pFile)
		{
			fwrite(pLog->time.c_str(), 1, pLog->time.length(), pFile);
			fwrite(pLog->info.c_str(), 1, pLog->info.length(), pFile);
			fwrite("\n", 1, 1, pFile);
			fclose(pFile);
			pFile = nullptr;
		}
	}
}
void TaskManager::SetPlateResult(LicPlateResult& plate_result, shared_ptr<cv::Mat> pImg)
{
	string strLog = console->info("[pmn] 接收到车牌数据[{}] conf[{}] ip[{}] lane[{}] GantryValue[{}] path[{}] 大车运动状态[{}]",
		plate_result.sLicPlate,plate_result.nBelieve, plate_result.sCamIp, plate_result.lane, m_plcinfo.gantryValue,
		plate_result.sFileName, m_plcInfoState.IsGantryMoving());
	AddMainLog(strLog);

	if (plate_result.lane <=0)
	{
		AddTaskLog(console->info("[pmn] 相机找不到车道:[{}]", plate_result.sCamIp));
		return;
	}
	// 无车不入主池：可暂存 pending，等 hasTruck 后再写入；其它门控失败仍直接丢弃
	{
		string failReason;
		if (!IsPmnCaptureWindow(plate_result.lane, &failReason))
		{
			const int ht = (plate_result.lane >= 1 && plate_result.lane <= 2)
				? m_plcInfoState.GetCurPLCInfo().hasTruck[plate_result.lane] : -1;
			const int holdMs = g_AppConfig.userCfg.pmnPlateEarlyHoldMs;
			if (failReason == "noTruck" && holdMs > 0 && plate_result.lane >= 1 && plate_result.lane <= 2)
			{
				SavePendingPlate(plate_result.lane, plate_result.sLicPlate,
					(float)plate_result.nBelieve, plate_result.sFileName, m_plcinfo.gantryValue);
				AddTaskLog(console->info("[pmn][gate][plate] 无车暂存 plate={} conf={} lane={} holdMs={} hasTruck={}",
					plate_result.sLicPlate, plate_result.nBelieve, plate_result.lane, holdMs, ht));
			}
			else
			{
				AddTaskLog(console->info("[pmn][gate][plate] 门控未过丢弃 plate={} conf={} lane={} reason={} hasTruck={}",
					plate_result.sLicPlate, plate_result.nBelieve, plate_result.lane, failReason, ht));
			}
			return;
		}
	}
	ClearPendingPlate(plate_result.lane);
	// 截取车号后6位进行比较
	std::string subpmn ="";
	if (plate_result.sLicPlate.length() > 2)
	{
		subpmn = plate_result.sLicPlate;//粤字占2字符
	}
	//std::string planTruckNum = m_planTruckNum.substr(2);//gd两个字符
	console->info("[pmn] 截取到车牌数据subpmn{}, planTruckNum{}", subpmn, m_planTruckNum);
	if (subpmn == m_planTruckNum)
	//if (plate_result.sLicPlate == m_planTruckNum)
	{
		m_planTruckMatch = true;
	}
	m_mtxPmnItem.lock();
	// 有车门控通过后写入缓存；发送时 GetPmnResult 按 gantry 窗口决定是否上传 TruckPlate
	m_arrPmnItem2[plate_result.lane].type = PLCInfo::TRUCK_TYPE_OUTER;
	m_arrPmnItem2[plate_result.lane].pmn = plate_result.sLicPlate;
	m_arrPmnItem2[plate_result.lane].conf = plate_result.nBelieve;
	m_arrPmnItem2[plate_result.lane].path = plate_result.sFileName;

	m_arrPmnItem2[plate_result.lane].GantryValue = m_plcinfo.gantryValue;

	console->info("[pmn] ==============");
	for (auto &item : m_arrPmnItem2)
	{
		console->info("[pmn] 车牌数据保存 lane[{}] GantryValue[{}] plate[{}]", item.first,item.second.GantryValue, item.second.pmn);
	}
	console->info("[pmn] ==============");
	m_mtxPmnItem.unlock();

	//把车牌相机的结果也按照识别方式进行管理。
	tPicItem pic_item;
	pic_item.pTaskMgr = this;
	pic_item.cam_ip = plate_result.sCamIp;
	pic_item.group = 0;
	pic_item.task_id = m_task_id;
	pic_item.ori_img_name = plate_result.sFileName;
	pic_item.ori_name = plate_result.sFileName;
	g_imgLabelMutex.lock();
	g_picNum++;
	pic_item.img_label = g_picNum;
	g_imgLabelMutex.unlock();

	/*AddTaskLog(console->info("[ice]车牌相机[{}]识别结果[{}/{}]",
		plate_result.sCamIp, plate_result.sLicPlate, plate_result.nBelieve));*/
	m_TruckNoPlateNum.plate_num = plate_result.sLicPlate;
	m_plateNum = plate_result.sLicPlate;

	//使用m_mtxTaskInfo锁
	std::lock_guard<std::mutex> lck(m_mtxTaskInfo);

	bool bInvalidPlate = false;
	string matchInvalid;
	for (auto &invalid_plate : g_AppConfig.arrInvalidPlate)
	{
		if (plate_result.sLicPlate.find(invalid_plate) != -1)
		{
			bInvalidPlate = true;
			matchInvalid = invalid_plate;
			break;
		}
	}
	for (auto& item : g_TruckListReply.body().ignor_truck_arr())
	{
		if (plate_result.sLicPlate.find(item.truck_plate()) != -1)
		{
			bInvalidPlate = true;
			matchInvalid = item.truck_plate();
			break;
		}
	}
	if (bInvalidPlate)
	{
		string strNumber;//sLicPlate中数字部分，如粤AB1234->1234,粤A1B234->234
		char szPlate[100] = {};
		sprintf_s(szPlate, "%s", plate_result.sLicPlate.c_str());
		int nLen = plate_result.sLicPlate.length();
		for (int nPos = nLen - 1; nPos > 0; nPos--)
		{
			if (!isNumber(szPlate[nPos]))
			{
				strNumber = szPlate + nPos + 1;
				break;
			}
		}
		//数字部分是否为内集卡车号
		bool bIsMatchInnerPlate = false;
		for (auto& item : g_TruckListReply.body().truck_arr())
		{
			if (strNumber == item.truck_plate())
			{
				bIsMatchInnerPlate = true;
				break;
			}
		}

		if (bIsMatchInnerPlate)
		{
			//车牌结果中包含屏蔽的字段，并且数字部分与内集卡车号匹配，认为是内集卡
			/*AddTaskLog(console->info("车牌相机[{}]识别结果[{}/{}]认为是内集卡[{}]",
				plate_result.sCamIp, plate_result.sLicPlate, plate_result.nBelieve, strNumber));*/

			bool bFindPlate = false;
			for (auto&t : m_TaskInfo.ocr().pmr_arr())
			{
				if (t.pmr().pmn() == strNumber)
				{
					bFindPlate = true;
					break;
				}
			}
			if (!bFindPlate)
			{
				PMObject *pmro = m_TaskInfo.mutable_ocr()->add_pmr_arr();
				pmro->set_group(0);
				pmro->mutable_pmr()->set_pmn(strNumber);
				pmro->mutable_pmr()->mutable_rect_pmn()->set_confidence(90);
				m_TaskInfo.set_pmn_result(1);
				//AddTaskLog(console->info("保存内集卡结果[{}]置信度[90]", strNumber));
			}
		}
		else
		{
			//车牌结果中包含屏蔽的字段，认为外集卡车牌结果无效
			/*AddTaskLog(console->info("车牌相机[{}]识别结果[{}/{}]含有[{}]认为车牌无效，不使用",
				plate_result.sCamIp, plate_result.sLicPlate, plate_result.nBelieve, matchInvalid));*/
		}
	}
	else
	{
		if (plate_result.nBelieve >= 60)
		{
			vector<LicPlateResult> *pArrRet = &m_arrPlateHead;
			if (plate_result.sLicPlate.find("挂") != -1)
				pArrRet = &m_arrPlateTail;
			bool bFind = false;
			for (auto&item : *pArrRet)
			{
				if (item.sLane == plate_result.sLane &&
					item.sCamIp == plate_result.sCamIp)
				{
					//更新结果，不比较置信度，使用最新的识别结果
					/*AddTaskLog(console->info("车牌相机[{}]更新结果 [{}/{}->{}/{}]",
						item.sCamIp, item.sLicPlate, item.nBelieve, plate_result.sLicPlate, plate_result.nBelieve));*/
					item.nBelieve = plate_result.nBelieve;
					item.sLicPlate = plate_result.sLicPlate;
					item.uiRecong = plate_result.uiRecong;
					item.strRecTime = plate_result.strRecTime;
					item.sFileName = plate_result.sFileName;
					bFind = true;
					break;
				}
			}
			if (!bFind)
			{
				//未找到，增加一个到vector中
				LicPlateResult item;
				item.nBelieve = plate_result.nBelieve;
				item.sLicPlate = plate_result.sLicPlate;
				item.sLane = plate_result.sLane;
				item.sCamIp = plate_result.sCamIp;
				item.uiRecong = plate_result.uiRecong;
				item.strRecTime = plate_result.strRecTime;
				item.sFileName = plate_result.sFileName;
				pArrRet->push_back(item);
				//AddTaskLog(console->info("车牌相机[{}]保存结果 [{}/{}]", plate_result.sCamIp, plate_result.sLicPlate, plate_result.nBelieve));
			}
		}
		else
		{
			/*AddTaskLog(console->info("车牌相机[{}]识别结果[{}/{}]置信度低于90认为车牌无效，不使用",
				plate_result.sCamIp, plate_result.sLicPlate, plate_result.nBelieve));*/
		}
	}
	//上传到FTP
	string ftp_src = g_AppConfig.userCfg.savePath + "\\"+ m_task_dir +"\\"+ plate_result.sFileName;
	string ftp_dst = m_task_dir + "\\"+plate_result.sFileName;
	if (plate_result.nBelieve < 60)
	{
		std::shared_ptr<ocrs::pb::GRpc_RImage> pRImage = std::make_shared<ocrs::pb::GRpc_RImage>();
		auto&img = *pRImage;
		img.mutable_image()->set_width(pImg->cols);
		img.mutable_image()->set_height(pImg->rows);
		img.mutable_image()->set_channel(pImg->channels());

		img.set_img_label(pic_item.img_label);
		img.set_cam_id(plate_result.sCamIp);

		GRpc_RecogParam * param = img.add_param();
		param->set_type(GRpc_RecogParam::PMN);
		tPicItem::tPicParam pic_param;
		pic_param.fRetConfidence = 0;
		pic_param.param = *param;
		pic_item.arrParam.push_back(pic_param);

		param = img.add_param();
		param->set_type(GRpc_RecogParam::PMN_QR);
		pic_param.fRetConfidence = 0;
		pic_param.param = *param;
		pic_item.arrParam.push_back(pic_param);
		//console->info("车牌相机[{}]识别车号 [{}/{}]", plate_result.sCamIp);

		//AddTaskLog(console->info("车牌相机[{}]发布识别照片label={}\n{}", plate_result.sCamIp, pic_item.img_label, ConvertChs(img.DebugString())));

		pic_item.pImgOri = pImg;
		img.mutable_image()->set_data(plate_result.vehPic.buf, plate_result.vehPic.size);
		RRIMAGELIST rrImageList;
		rrImageList.push_back(pRImage);
		m_RImageList.add(rrImageList);
	}
	else
	{
		pic_item.crs_deal = true;
		tPicItem::tPicParam pic_param;//设置识别参数和置信度
		pic_param.param.set_type(GRpc_RecogParam_G_RType_ZERO);
		pic_param.fRetConfidence = plate_result.nBelieve;
		pic_item.arrParam.push_back(pic_param);
	}
	g_mtxPicItem.lock();
	g_arrPicItem.push_back(pic_item);
	g_mtxPicItem.unlock();
	//g_pPicSave->SavePic(pImg, ftp_src, true, pic_item.img_label, ftp_dst);
	if (!bInvalidPlate)
		UpdatePlateResult();
}

// THPM 流识别入池：只记样本（号+单帧conf+当时gantry），不算分；PIIS 请求时按请求瞬间 gantry 窗口聚合。
void TaskManager::SetTruckResult(ocrs::pb::THPMResult& ret)
{
	console->info("[pmn] TH pmn[{}] image:[{}]",ret.pmr().pmn(),ret.image().size());
	ocrs::pb::THPMResult retTemp = ret;
	retTemp.mutable_image()->clear();
	AddTaskLog(console->info("[pmn] 接收车顶号识别结果THPMResult {}", retTemp.DebugString()));
	AddMainLog(console->info("[pmn] 接收车顶号识别结果THPMResult {}", retTemp.DebugString()));

	int laneTmp = 0;
	for (auto &cfg:m_pDevMgr->gunCfgList)
	{
		if (cfg.first == ret.camera_name())
		{
			console->info("[pmn] TH ip[{}]--[{}]", cfg.first, ret.camera_name());
			laneTmp = cfg.second->lane;
			break;
		}
	}
	if (laneTmp <=0)
	{
		AddTaskLog(console->info("[pmn] 相机找不到车道:[{}]", ret.camera_name()));
		return;
	}

	if (ret.pmr().pmn().empty())
	{
		//无结果
		return;
	}
	string saveImgName = SpliceFileName(ret.camera_name(), "THPM_");

	string pmn = ret.pmr().pmn();
	GetTruckNumFromMap(pmn);
	if (pmn.find("?") != string::npos)
	{
		AddTaskLog(console->info("[pmn] 结果无效[{}] ", pmn));
		return;
	}

	// 大车移动也入样本，PIIS 请求时再按请求瞬间 gantry 窗口算分（不要求停稳）。

	float conf = ret.pmr().rect_pmn().confidence();
	if (conf < 0.8)
	{
		console->info("[pmn] 车顶号置信度低不保存,{} {}", pmn, conf);
		return;
	}

	// 门控：默认仅 hasTruck。中间车票多靠拉号时窗口聚合，不靠行驶丢弃。
	{
		string failReason;
		if (!IsPmnCaptureWindow(laneTmp, &failReason))
		{
			AddTaskLog(console->info("[pmn][gate][THPM] 门控未过丢弃 pmn={} conf={} lane={} reason={} moving={} gantry={} hasTruck={}",
				pmn, conf, laneTmp, failReason,
				m_plcInfoState.IsGantryMoving(),
				m_plcInfoState.GetCurPLCInfo().gantryValue,
				m_plcInfoState.GetCurPLCInfo().hasTruck[laneTmp]));
			return;
		}
	}

	//已上传的结果判断是否一致
	/*if (pmn == m_planTruckNum)
	{
		m_planTruckMatch = true;
	}*/
	bool bValidPmn = true;
	for (auto&c : pmn)
	{
		if (c == '?')
		{
			c = 'x';
			bValidPmn = false;
		}
	}
	//判断车顶号识别结果是否发生变化，不同时才进行数据缓存，防止集卡长时间等待导致缓存太多数据占用太多内存

	saveImgName += pmn;
	saveImgName += "_ORI.jpg";

	tPicItem pic_item;
	pic_item.pTaskMgr = this;
	pic_item.cam_ip = ret.camera_name();
	pic_item.group = 0;
	pic_item.task_id = m_task_id;
	pic_item.ori_img_name = saveImgName;
	pic_item.ori_name = saveImgName;
	g_imgLabelMutex.lock();
	g_picNum++;
	pic_item.img_label = g_picNum;
	g_imgLabelMutex.unlock();

	pic_item.crs_deal = true;
	tPicItem::tPicParam pic_param;//设置识别参数和置信度
	pic_param.param.set_type(GRpc_RecogParam_G_RType_PMN);
	pic_param.fRetConfidence = ret.pmr().rect_pmn().confidence();
	pic_item.arrParam.push_back(pic_param);

	g_mtxPicItem.lock();
	g_arrPicItem.push_back(pic_item);
	int nItemSize = g_arrPicItem.size();
	g_mtxPicItem.unlock();
	AddMainLog(console->info("接收车顶号识别结果 g_arrPicItem={}", nItemSize));

	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	PmnResult*pPmn = AddPmnRet(laneTmp, pmn, conf);

	// 入池成功：本条样本的单帧 conf + 当时 gantry，不累加
	AddTaskLog(console->info("[pmn][THPM] 记样本 pmn={} conf={} lane={} gantry={} poolSize={} moving={}",
		pmn, conf, laneTmp,
		pPmn ? pPmn->GantryValue : 0,
		pPmn ? (int)m_arrPmnItem[laneTmp].size() : 0,
		m_plcInfoState.IsGantryMoving()));

	//要车号结果且没发送过结果,是否跟计划车号一致，一致直接发送
	if (m_taskState == TASKSTART)
	{
		//保存原图
		if (!ret.image().empty())
		{
			//保存车顶号识别结果原始图片
			shared_ptr<Mat> pImg = make_shared<Mat>();
			cv::Mat jpeg(1, ret.image().length(), CV_8UC1, (BYTE*)ret.image().c_str());
			cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());

			string ftp_src = g_AppConfig.userCfg.savePath + "\\" + m_task_dir + "\\" + saveImgName;
			string ftp_dst = m_task_dir + "\\" + saveImgName;
			g_pPicSave->SavePic(pImg, ftp_src, nullptr, nullptr, true, pic_item.img_label, ftp_dst);

			pPmn->path = ftp_src;
		}

		//保存识别后的
		// 解码JPEG图片
		cv::Mat jpeg1(1, ret.image().length(), CV_8UC1, (unsigned char *)ret.image().data());
		cv::Mat rgbImage;
		cv::imdecode(jpeg1, cv::IMREAD_COLOR, &rgbImage);

		cv::rectangle(rgbImage,
			cv::Rect(ret.pmr().rect_pmn().rect().x(), ret.pmr().rect_pmn().rect().y(),
				ret.pmr().rect_pmn().rect().w(), ret.pmr().rect_pmn().rect().h()),
			cv::Scalar(255, 0, 0),
			2,
			cv::LINE_8,
			0);
		cv::rectangle(rgbImage,
			cv::Rect(ret.rect_th().rect().x(), ret.rect_th().rect().y(),
				ret.rect_th().rect().w(), ret.rect_th().rect().h()),
			cv::Scalar(255, 0, 0),
			2,
			cv::LINE_8,
			0);

		//图片上添加结果文字
		std::string str1 = "pmn:" + ret.pmr().pmn();
		cv::putText(rgbImage, str1, cv::Point(100, 200),
			cv::FONT_HERSHEY_SIMPLEX,
			3.0,
			cv::Scalar(0, 0, 255),
			3.0);

		std::string str2 = "conf:" + to_string(ret.pmr().rect_pmn().confidence());
		cv::putText(rgbImage, str2, cv::Point(100, 300),
			cv::FONT_HERSHEY_SIMPLEX,
			3.0,
			cv::Scalar(0, 0, 255),
			3.0);

		std::string str3 = "type:U";
		if (ret.truck_type() == THPMResult_TRUCK_TYPE_INNER)
		{
			str3 = "type:INNER";
			//车头方向固定，内集卡车顶号区域应该在图片的上部分，如果车顶号出现在图片下半部分，认为无效
			/*if (ret.pmr().rect_pmn().rect().y() > rgbImage.rows / 2)
			{
				AddTaskLog(console->info("[crs pmn]内集卡车顶号识别结果无效 {} (识别区域在图片下半部分)", ret.pmr().pmn()));
				bValidPmn = false;
			}*/
		}
		else if (ret.truck_type() == THPMResult_TRUCK_TYPE_OUTER)
			str3 = "type:OUTER";
		cv::putText(rgbImage, str3, cv::Point(100, 400),
			cv::FONT_HERSHEY_SIMPLEX,
			3.0,
			cv::Scalar(0, 0, 255),
			3.0);

		//保存图片到本地
		saveImgName = SpliceFileName(ret.camera_name(), "THPM_");
		saveImgName += pmn;
		imwrite(g_AppConfig.userCfg.savePath + "\\" + m_task_dir + "\\" + saveImgName + "_REC.jpg", rgbImage);

		string sendPmn = "";
	}
}

// 流识别入库门控：行驶中允许；仅查 hasTruck。不因 moving 拒绝（路过票要存）。
bool TaskManager::IsPmnCaptureWindow(int lane, string* pFailReason) const
{
	if (!g_AppConfig.userCfg.pmnWindowEnable)
	{
		return true;
	}
	if (lane < 1 || lane > 2)
	{
		if (pFailReason) *pFailReason = "invalidLane";
		return false;
	}
	const PLCInfo& plc = m_plcInfoState.GetCurPLCInfo();

	// 车道无车则不入池（与大车是否移动无关）
	if (plc.hasTruck[lane] != 1)
	{
		if (pFailReason) *pFailReason = "noTruck";
		return false;
	}

	// pmnWindowGantryDelta 只用于 GetPmnResult 按请求位置选号，不参与入库拦截
	return true;
}

// 按 hasTruck 无车消抖后清指定车道车顶号样本 + 车牌；
void TaskManager::ClearPmnCache(int lane, const string& reason)
{
	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	int nSize = 0;
	auto it = m_arrPmnItem.find(lane);
	if (it != m_arrPmnItem.end())
	{
		for (auto& ret : it->second)
		{
			AddTaskLog(console->info("[pmn][gate] 清理车号结果 {}", ret.to_string()));
		}
		nSize = (int)it->second.size();
		it->second.clear();
	}
	auto it2 = m_arrPmnItem2.find(lane);
	if (it2 != m_arrPmnItem2.end())
	{
		AddTaskLog(console->info("[pmn][gate] 清理车牌结果 {}", it2->second.to_string()));
		it2->second.Clear();
	}
	if (lane >= 1 && lane <= 2 && m_pendingPlate[lane].valid)
	{
		AddTaskLog(console->info("[pmn][gate] 清理暂存车牌 lane={} plate={}",
			lane, m_pendingPlate[lane].pmn));
		m_pendingPlate[lane].Clear();
	}
	m_TaskInfo.mutable_ocr()->clear_pmr_arr();
	AddTaskLog(console->info("[pmn][gate] ClearPmnCache lane={} reason={} poolSize={}",
		lane, reason, nSize));
	m_gantryAnchor = -9999;
}

// 跟踪各车道入库门控状态（TaskTrigger 每帧 PLC 更新后调用）。
// 门控条件仅 hasTruck；大车移动不影响。无车后经 pmnNoTruckClearFrames 帧消抖才清车顶号池与车牌。
// m_pmnWindowInLane 为带迟滞的逻辑状态：消抖期间可为 true 而 hasTruck 已为 0；
// 此时 IsPmnCaptureWindow 仍按实时 hasTruck 拦截新样本入主池（车牌可进 pending）。
void TaskManager::UpdatePmnWindowState()
{
	if (!g_AppConfig.userCfg.pmnWindowEnable)
	{
		return;
	}
	const PLCInfo& plc = m_plcInfoState.GetCurPLCInfo();
	int clearFrames = g_AppConfig.userCfg.pmnNoTruckClearFrames;
	if (clearFrames <= 0)
	{
		clearFrames = 1; // 0=不去抖，等同 1 帧即清
	}
	for (int lane = 1; lane <= 2; ++lane)
	{
		bool hasTruck = (plc.hasTruck[lane] == 1); // 当前帧 PLC 有/无车
		bool wasIn = m_pmnWindowInLane[lane];      // 上一帧逻辑门控状态

		// 有车说明在作业中，无车计数器归0，如果是第一次有车，打印日志进入可入库
		if (hasTruck)
		{
			// 有车：维持/进入门控；消抖中途恢复有车则取消计数，保留池中历史样本
			if (m_hasTruckZeroFrames[lane] > 0)
			{
				AddTaskLog(console->info("[pmn][gate] 无车消抖取消 lane={} zeroFrames={}",
					lane, m_hasTruckZeroFrames[lane]));
			}
			m_hasTruckZeroFrames[lane] = 0;
			if (!wasIn)
			{
				m_gantryAnchor = plc.gantryValue; // 仅诊断日志用，不参与入库拦截
				AddTaskLog(console->info("[pmn][gate] 进入可入库 lane={} gantry={} moving={} hasTruck={}",
					lane, plc.gantryValue, m_plcInfoState.IsGantryMoving(), plc.hasTruck[lane]));
			}
			m_pmnWindowInLane[lane] = true;
			// 每帧有车都尝试提交暂存车牌（覆盖进车前早识别）
			TryCommitPendingPlate(lane);
		}
		else if (wasIn)
		{
			// 无车但仍在门控内：累加无车帧数，未达阈值前保持 wasIn=true 不清池
			m_hasTruckZeroFrames[lane]++;
			if (m_hasTruckZeroFrames[lane] >= clearFrames)
			{
				AddTaskLog(console->info("[pmn][gate] 离开可入库(无车消抖达标) lane={} zeroFrames={} → ClearPmnCache",
					lane, m_hasTruckZeroFrames[lane]));
				ClearPmnCache(lane, "noTruckDebounced");
				m_pmnWindowInLane[lane] = false;
				m_hasTruckZeroFrames[lane] = 0;
			}
			else
			{
				AddTaskLog(console->info("[pmn][gate] 无车消抖中 lane={} zeroFrames={}/{}",
					lane, m_hasTruckZeroFrames[lane], clearFrames));
			}
		}
		else
		{
			// 无车且不在门控内：空闲态；过期暂存丢掉，避免过路牌占坑
			DropExpiredPendingPlate(lane);
			m_hasTruckZeroFrames[lane] = 0;
			m_pmnWindowInLane[lane] = false;
		}
	}
}

int64_t TaskManager::PmnSteadyMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TaskManager::SavePendingPlate(int lane, const string& pmn, float conf, const string& path, int gantry)
{
	if (lane < 1 || lane > 2)
	{
		return;
	}
	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	m_pendingPlate[lane].valid = true;
	m_pendingPlate[lane].pmn = pmn;
	m_pendingPlate[lane].conf = conf;
	m_pendingPlate[lane].path = path;
	m_pendingPlate[lane].gantry = gantry;
	m_pendingPlate[lane].tickMs = PmnSteadyMs();
}

void TaskManager::ClearPendingPlate(int lane)
{
	if (lane < 1 || lane > 2)
	{
		return;
	}
	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	m_pendingPlate[lane].Clear();
}

void TaskManager::CommitPendingPlateUnderLock(int lane)
{
	if (lane < 1 || lane > 2)
	{
		return;
	}
	PendingPlate& pend = m_pendingPlate[lane];
	if (!pend.valid)
	{
		return;
	}
	const int holdMs = g_AppConfig.userCfg.pmnPlateEarlyHoldMs;
	const int64_t age = PmnSteadyMs() - pend.tickMs;
	if (holdMs <= 0 || age > holdMs)
	{
		AddTaskLog(console->info("[pmn][gate][plate] 暂存过期丢弃 plate={} lane={} ageMs={} holdMs={}",
			pend.pmn, lane, (int)age, holdMs));
		pend.Clear();
		return;
	}
	m_arrPmnItem2[lane].type = PLCInfo::TRUCK_TYPE_OUTER;
	m_arrPmnItem2[lane].pmn = pend.pmn;
	m_arrPmnItem2[lane].conf = pend.conf;
	m_arrPmnItem2[lane].path = pend.path;
	m_arrPmnItem2[lane].GantryValue = pend.gantry;
	m_plateNum = pend.pmn;
	m_TruckNoPlateNum.plate_num = pend.pmn;
	if (pend.pmn.length() > 2 && pend.pmn == m_planTruckNum)
	{
		m_planTruckMatch = true;
	}
	AddTaskLog(console->info("[pmn][gate][plate] 暂存入主池 plate={} conf={} lane={} ageMs={} gantry={}",
		pend.pmn, pend.conf, lane, (int)age, pend.gantry));
	pend.Clear();
}

void TaskManager::TryCommitPendingPlate(int lane)
{
	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	CommitPendingPlateUnderLock(lane);
}

void TaskManager::DropExpiredPendingPlate(int lane)
{
	if (lane < 1 || lane > 2)
	{
		return;
	}
	std::lock_guard<std::mutex> lck(m_mtxPmnItem);
	PendingPlate& pend = m_pendingPlate[lane];
	if (!pend.valid)
	{
		return;
	}
	const int holdMs = g_AppConfig.userCfg.pmnPlateEarlyHoldMs;
	const int64_t age = PmnSteadyMs() - pend.tickMs;
	if (holdMs <= 0 || age > holdMs)
	{
		AddTaskLog(console->info("[pmn][gate][plate] 空闲暂存过期 plate={} lane={} ageMs={} holdMs={}",
			pend.pmn, lane, (int)age, holdMs));
		pend.Clear();
	}
}

// 追加一条车顶号样本（号+单帧conf+当时gantry），不按号合并。
// fromQr=true：来自二维码；GetPmnResult 内集卡匹配成功时 StatusCode 用 1 而非 0。
TaskManager::PmnResult* TaskManager::AddPmnRet(int lane, string pmn, float conf, bool fromQr)
{
	if (m_arrPmnItem.find(lane) == m_arrPmnItem.end())
	{
		m_arrPmnItem[lane] = vector<PmnResult>();
	}
	PmnResult sample;
	sample.Clear();
	sample.pmn = pmn;
	sample.conf = conf;
	sample.nCount = 1;
	sample.type = PLCInfo::TRUCK_TYPE_INNER;
	sample.GantryValue = m_plcInfoState.GetCurPLCInfo().gantryValue;
	sample.fromQr = fromQr;
	if (fromQr)
		sample.qr = pmn;
	m_arrPmnItem[lane].push_back(sample);
	PmnResult* pPmn = &m_arrPmnItem[lane].back();
	AddTaskLog(console->info("[pmn] 记样本 lane={} pmn={} conf={} gantry={} fromQr={} poolSize={}",
		lane, pPmn->pmn, pPmn->conf, pPmn->GantryValue, pPmn->fromQr, (int)m_arrPmnItem[lane].size()));
	return pPmn;
}

void TaskManager::SetTruckStatus(ocrs::pb::TRUCKSTATUS& truck_ststus)
{
	int laneTmp = 0;
	for (auto &cfg : m_pDevMgr->gunCfgList)
	{
		if (cfg.first == truck_ststus.camera_name())
		{
			console->info("[TruckStatus] TH ip[{}]--[{}]", cfg.first, truck_ststus.camera_name());
			laneTmp = cfg.second->lane;
			break;
		}
	}
	if (laneTmp <= 0)
	{
		if (truck_ststus.camera_name().find("Right")!=std::string::npos)
		{
			laneTmp = 1;
		}
		else if (truck_ststus.camera_name().find("Left") != std::string::npos)
		{
			laneTmp = 2;
		}
		else {
			AddTaskLog(console->info("[TruckStatus] 相机找不到车道:[{}]", truck_ststus.camera_name()));
			return;
		}

	}

	old_truckStatus[laneTmp] = cur_truckStatus[laneTmp];
	cur_truckStatus[laneTmp] = truck_ststus.truck_status();
	AddTaskLog(console->info("[TruckStatus] SetTruckStatus[{}]:[{}]->[{}]", laneTmp, old_truckStatus[laneTmp] ,cur_truckStatus[laneTmp]));
}
void TaskManager::UpdatePlateResult()
{
	if (m_arrPlateHead.size())
	{
		m_PlateResult = m_arrPlateHead[m_arrPlateHead.size() - 1];
		AddTaskLog(console->info("保存车牌号结果 [{}/{}]", m_PlateResult.sLicPlate, m_PlateResult.nBelieve));
	}
	else if (m_arrPlateTail.size())
	{
		m_PlateResult = m_arrPlateTail[m_arrPlateTail.size() - 1];
		AddTaskLog(console->info("保存车架号结果 [{}/{}]", m_PlateResult.sLicPlate, m_PlateResult.nBelieve));
	}
}

struct tTriggerEvent
{
	string cam_ip;
	string evt_name;
	Event  *pEvent;
	tTriggerEvent(string ip, string name, Event *p)
	{
		cam_ip = ip;
		evt_name = name;
		pEvent = p;
	}
	tTriggerEvent(){}
};

void TaskManager::SetJobTrigger(gui::pb::JobTrigger& trigger)
{
	m_bRecvJobTrigger = true;
	m_bRecvJobTriggerSendLog = true;
	m_JobTrigger.CopyFrom(trigger);
	auto strLog = console->info("接收ECS触发：\n{}", ConvertChs(trigger.DebugString()));
	AddTaskLog(strLog);
	string strTriggerPath = m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_" + trigger.body().job_id() + "_trigger.txt";
	WriteStringToTxt(strTriggerPath, strLog);
	char szFileName[200] = {};
	//任务号_jobid_作业标识_车头方向_压箱位置
	sprintf_s(szFileName, "%s_%s_%s_%s_%s.txt",
		m_task_id.c_str(),
		trigger.body().job_id().c_str(),
		trigger.body().flag().c_str(),
		trigger.body().truck_dir().c_str(),
		trigger.body().cont_pos().c_str());
	m_strJobPath = m_TaskInfo.local_snapshot_location() + szFileName;

	//车道和作业类型
	m_spreader = m_plcInfoState.GetCurPLCInfo().GetSpreadMode();
	m_TaskInfo.set_work_lane(m_lane_id);
	m_TaskInfo.set_spreader(m_spreader);
	//车头方向，0=无效；1=左（反向）；2=右（正向）
	if (trigger.body().truck_dir() == "1")
		m_TaskInfo.set_truck_head_dir("LEFT");
	else if (trigger.body().truck_dir() == "2")
		m_TaskInfo.set_truck_head_dir("RIGHT");
	else
		m_TaskInfo.set_truck_head_dir("Unknow");
	//根据吊具开闭锁判断作业类型：开锁为收箱，否则为发箱 2022.11.25 按照triggerflag 判断类型
	//if (m_plcInfoState.GetCurPLCInfo().lockStatus == PLCInfo::LOCKED)
	//{
	//	m_TaskInfo.set_work_type(YardToTruck);
	//	AddTaskLog(console->info("发箱作业"));
	//}
	//else
	//{
	//	m_TaskInfo.set_work_type(TruckToYard);
	//	AddTaskLog(console->info("收箱作业"));
	//}

	string info = "ECS请求 ";
	info += trigger.body().flag();
	if (trigger.body().flag() == "1")
	{
		info += " 车号、箱号 ";
		if (g_AppConfig.userCfg.only_pmn)
		{
			info += " (无箱号功能) ";
		}
		m_TaskInfo.set_work_type(TruckToYard);
		AddTaskLog(console->info("收箱作业"));
	}
	else if (trigger.body().flag() == "2")
	{
		info += " 发箱作业箱号 ";
		if (g_AppConfig.userCfg.only_pmn)
		{
			info += " (无箱号功能) ";
		}
		m_TaskInfo.set_work_type(YardToTruck);
		AddTaskLog(console->info("发箱作业"));
	}
	else if (trigger.body().flag() == "3")
	{
		info += " 车号 ";
		m_TaskInfo.set_work_type(YardToTruck);
		AddTaskLog(console->info("发箱作业"));
	}
	else if (trigger.body().flag() == "4")
	{
		info += " 收箱作业箱号 ";
		if (g_AppConfig.userCfg.only_pmn)
		{
			info += " (无箱号功能) ";
		}
		m_TaskInfo.set_work_type(YardToTruck);
		AddTaskLog(console->info("发箱作业"));
	}
	else if (trigger.body().flag() == "5")
	{
		info += " 内集卡车顶号、箱号 ";
		if (g_AppConfig.userCfg.only_pmn)
		{
			info += " (无箱号功能) ";
		}
		m_TaskInfo.set_work_type(TruckToYard);
		AddTaskLog(console->info("收箱作业"));
	}
	else if (trigger.body().flag() == "6")
	{
		info += " 内集卡车顶号 ";
		m_TaskInfo.set_work_type(YardToTruck);
		AddTaskLog(console->info("发箱作业"));
	}
	else if (trigger.body().flag() == "7")
	{
		info += " 外集卡车牌号、箱号 ";
		if (g_AppConfig.userCfg.only_pmn)
		{
			info += " (无箱号功能) ";
		}
		m_TaskInfo.set_work_type(TruckToYard);
		AddTaskLog(console->info("收箱作业"));
	}
	else if (trigger.body().flag() == "8")
	{
		info += " 外集卡车牌号 ";
		m_TaskInfo.set_work_type(YardToTruck);
		AddTaskLog(console->info("发箱作业"));
	}
	AddTaskLog(console->info(info));
	WriteStringToTxt(m_strJobPath, GetTimeString() + " ");
	WriteStringToTxt(m_strJobPath, strLog);
	WriteStringToTxt(m_strJobPath, info + "\n");
	AddMainLog(strLog);
	AddMainLog(info);

	ActionCameraByTrigger(trigger);

	std::thread th = std::thread{
		[=]()
		{
			static bool bRun = false;
			if (bRun)
			{
				AddTaskLog(console->info("发送结果到ECS已运行"));
				return;
			}
			bRun = true;
			g_imgLabelMutex.lock();
			g_nRecvRetLabel = -1;
			g_imgLabelMutex.unlock();
			for (int nWaitMs = 0; nWaitMs <= g_AppConfig.userCfg.ecs_ms;)
			{
				nWaitMs += 200;
				//如果接收到的CRS识别结果序号大于发送的序号，认为识别结束，不再等待，直接发送结果
				g_imgLabelMutex.lock();
				if (g_nRecvRetLabel >= g_picNum)
				{
					g_imgLabelMutex.unlock();
					AddTaskLog(console->info("接收到全部识别结果，发送到ECS"));
					break;
				}
				g_imgLabelMutex.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}

			AddTaskLog(console->info("发送结果到ECS开始"));
			SendResult(true);
			AddTaskLog(console->info("发送结果到ECS结束"));

			//m_taskState = TASKEND;//以发生结果后认为作业结束
			bRun = false;
		}
	};
	th.detach();
}

void TaskManager::ActionCameraByTrigger(gui::pb::JobTrigger& trigger)
{
	//车头方向，0 = 无效；1 = 左（反向）；2 = 右（正向）
	string trigger_dir = trigger.body().truck_dir();
	//作业位置，F=前箱位；M=中箱位；A=后箱位；U=无效
	string trigger_pos = trigger.body().cont_pos();
	string trigger_spreader = m_plcInfoState.GetCurPLCInfo().spreaderMode == PLCInfo::SINGLE20 ? "20FT" : "40FT";
	//查找作业需要工作的相机
	vector<tTriggerEvent> arrCamEvent;
	for (auto&guncfg : m_pDevMgr->gunCfgList)
	{
		for (auto& evt : guncfg.second->events.eventList)
		{
			if (evt.second.evtType == "TRIGGER")
			{
				if (evt.second.spreader.empty() &&
					evt.second.truck_dir.empty() &&
					evt.second.cont_pos.empty())
				{
					//未指定spreader和truck_dir，直接保存
					arrCamEvent.push_back(tTriggerEvent{ guncfg.second->ip, evt.first, &evt.second });
					AddTaskLog(console->info("保存相机[{}]事件[{}](事件未设置筛选参数)", guncfg.first, evt.first));
				}
				else
				{
					bool bMatch = true;
					string strInfo;
					if (evt.second.spreader.length())
					{
						if (evt.second.spreader == trigger_spreader)
						{
							bMatch &= true;
							strInfo += fmt::format("[匹配吊具类型 cfg={} trigger={}]", evt.second.spreader, trigger_spreader);
						}
						else
						{
							bMatch &= false;
							strInfo += fmt::format("[不匹配吊具类型 cfg={} trigger={}]", evt.second.spreader, trigger_spreader);
						}
					}
					if (evt.second.truck_dir.length())
					{
						if (trigger_dir == "0")
						{
							bMatch &= true;
							strInfo += fmt::format("[无效车头方向(认为匹配) cfg={} trigger={}]", evt.second.truck_dir, trigger_dir);
						}
						else if (evt.second.truck_dir == trigger_dir)
						{
							bMatch &= true;
							strInfo += fmt::format("[匹配车头方向 cfg={} trigger={}]", evt.second.truck_dir, trigger_dir);
						}
						else
						{
							bMatch &= false;
							strInfo += fmt::format("[不匹配车头方向 cfg={} trigger={}]", evt.second.truck_dir, trigger_dir);
						}
					}
					if (evt.second.cont_pos.length())
					{
						if (trigger_pos == "U")
						{
							bMatch &= true;
							strInfo += fmt::format("[无效压箱位置(认为匹配) cfg={} trigger={}]", evt.second.cont_pos, trigger_pos);
						}
						else if (evt.second.cont_pos == trigger_pos)
						{
							bMatch &= true;
							strInfo += fmt::format("[匹配压箱位置 cfg={} trigger={}]", evt.second.cont_pos, trigger_pos);
						}
						else
						{
							bMatch &= false;
							strInfo += fmt::format("[不匹配压箱位置 cfg={} trigger={}]", evt.second.cont_pos, trigger_pos);
						}
					}
					if (bMatch)
					{
						arrCamEvent.push_back(tTriggerEvent{ guncfg.second->ip, evt.first, &evt.second });
						AddTaskLog(console->info("保存相机[{} {}]事件[{}]，{}", guncfg.first, guncfg.second->ip, evt.first, strInfo));
					}
					else
					{
						AddTaskLog(console->warn("未保存相机[{} {}]事件[{}]，{}", guncfg.first, guncfg.second->ip, evt.first, strInfo));
					}
				}
			}
		}
	}
	for (auto&item : arrCamEvent)
	{
		auto itr = m_pDevMgr->gunDevList.find(item.cam_ip);
		if (itr != m_pDevMgr->gunDevList.end())
		{
			itr->second->Action(item.evt_name, item.pEvent);
		}
	}
}


void TaskManager::SetDoorOpenRet(int img_label, DoorOPENResult& dor)
{
	string cam_name = GetCamNameByImgLabel(img_label);
	if (dor.confidence() < g_AppConfig.userCfg.door_open_conf)
	{
		AddTaskLog(console->info("箱门打开状态，未使用：{} 置信度 {} 低于 {}", cam_name, dor.confidence(), g_AppConfig.userCfg.door_open_conf));
		return;
	}
	//置信度大于设定值，使用最新打开结果
	AddTaskLog(console->info("接收箱门打开状态：{} type={} conf={}", cam_name, dor.type(), dor.confidence()));
	m_mtxTaskInfo.lock();
	if (m_mapCamOpenRet.find(cam_name) != m_mapCamOpenRet.end())
	{
		tDoorOpenRet &open_ret = m_mapCamOpenRet[cam_name];
		if (open_ret.open_type == dor.type())
		{
			//最新识别结果与保存结果一致，计数+1
			open_ret.ret_count++;
			AddTaskLog(console->info("箱门打开状态，累计：{} type={},count={}", cam_name, dor.type(), open_ret.ret_count));
		}
		else
		{
			AddTaskLog(console->info("箱门打开状态，更新：{} type={}->{}", cam_name, open_ret.open_type, dor.type()));
			//最新识别结果与保存结果不一致，保存最新结果，计数重置为1
			open_ret.open_type = dor.type();
			open_ret.ret_count = 1;
		}
	}
	else
	{
		tDoorOpenRet open_ret;
		open_ret.open_type = dor.type();
		open_ret.ret_count = 1;
		m_mapCamOpenRet[cam_name] = open_ret;
		AddTaskLog(console->info("箱门打开状态，增加：{} type={}", cam_name, dor.type()));
	}

	m_mtxTaskInfo.unlock();
	UpdateDoorOpenRetToPlc();
}

void TaskManager::SetDoorDirRet(int img_label, DoorResult& dr)
{
	string cam_name = GetCamNameByImgLabel(img_label);
	bool bIsFrontDoor = dr.type() == DoorResult::FRONT_DOOR;
	AddTaskLog(console->info("箱门打开状态，接收箱门方向：{} type={} conf={}", cam_name, EnumTransform::GetDoorResult(dr.type()), dr.confidence()));
	m_mtxTaskInfo.lock();
	if (m_mapCamOpenRet.find(cam_name) != m_mapCamOpenRet.end())
	{
		//保存置信度高的结果
		tDoorOpenRet &open_ret = m_mapCamOpenRet[cam_name];
		if (open_ret.conf < dr.confidence())
		{
			AddTaskLog(console->info("箱门打开状态，箱门方向更新：{} conf={}->{} front_door={}->{}", cam_name, open_ret.conf, dr.confidence(), open_ret.front_door, bIsFrontDoor));
			open_ret.conf = dr.confidence();
			open_ret.front_door = bIsFrontDoor;
		}
		else
		{
			AddTaskLog(console->info("箱门打开状态，箱门方向未更新：{} conf={} front_door={}", cam_name, open_ret.conf, open_ret.front_door));
		}
	}
	else
	{
		tDoorOpenRet open_ret;
		open_ret.front_door = bIsFrontDoor;
		open_ret.conf = dr.confidence();
		m_mapCamOpenRet[cam_name] = open_ret;
		AddTaskLog(console->info("箱门打开状态，箱门方向增加：{} conf={} front_door={}", cam_name, open_ret.conf, bIsFrontDoor));
	}
	m_mtxTaskInfo.unlock();

	//保存相机识别的箱门结果
	m_mtxCamDoorRet.lock();
	if (m_mapCamDoorRet.find(cam_name) != m_mapCamDoorRet.end())
	{
		//保存置信度高的结果
		tDoorRet &ret = m_mapCamDoorRet[cam_name];
		if (ret.conf < dr.confidence())
		{
			AddTaskLog(console->info("箱门方向更新：{} conf={}->{} type={}->{}", cam_name, ret.conf, dr.confidence(), EnumTransform::GetDoorResult(ret.type), EnumTransform::GetDoorResult(dr.type())));
			ret.conf = dr.confidence();
			ret.type = dr.type();
		}
		else
		{
			AddTaskLog(console->info("箱门方向未更新：{} conf={} type={}", cam_name, ret.conf, EnumTransform::GetDoorResult(ret.type)));
		}
	}
	else
	{
		tDoorRet ret;
		ret.type = dr.type();
		ret.conf = dr.confidence();
		m_mapCamDoorRet[cam_name] = ret;
		AddTaskLog(console->info("箱门方向增加：{} conf={} type={}", cam_name, ret.conf, EnumTransform::GetDoorResult(ret.type)));
	}
	m_mtxCamDoorRet.unlock();
}

int TaskManager::GetDoorDirCorrect()
{
	/*
	箱门(BACK_DOOR)朝向是否正确，现场单向作业，车头侧为大贝位，车尾侧为小贝位，箱门朝向要求朝向车尾（小贝位）方向
	箱门朝向正确（满足BACK_DOOR在车尾侧）为1，否则为2
	40尺作业：
	朝向错误：车头检测到箱门、车尾检测到非箱门，除此情况，其他认为正确
	20尺作业：
	收箱使用单个门腿球机识别，存在双20尺做前或后箱，有一个面被遮挡，前箱使用车头相机识别，后箱使用车尾相机识别
	朝向正确：前箱作业，使用车头侧相机识别到非箱门；后箱作业，使用车尾侧相机识别到箱门，其他为错误
	发箱使用吊具摄像头识别
	朝向错误：车头检测到箱门、车尾检测到非箱门，除此情况，其他认为正确
	*/
	std::lock_guard<mutex> lck(m_mtxCamDoorRet);
	float conf=0;
	for (auto& door : m_mapCamDoorRet)
	{
		AddTaskLog(console->info("箱门方向结果：{} conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
		if (door.second.conf<conf)
		{
			conf = door.second.conf;
		}
	}
	if (Sany_Code.door_code != DoorDir_TimeoutOffline && Sany_Code.door_code != DoorOpen_TimeoutOnline)
	{
			if (conf<0.8)
			{
				Sany_Code.door_code = DoorDir_LowConfidence;
			}
			else
			{
				Sany_Code.door_code = DoorDir_Normal;
			}

	}
	int nSpreader = m_plcInfoState.GetCurPLCInfo().spreaderMode;//吊具类型
	int nWorkType = m_plcInfoState.GetCurPLCInfo().workType;//进出箱作业
	int nContPos = m_plcInfoState.GetCurPLCInfo().cpsCnPos;//压箱位置
	if (PLCInfo::SINGLE20 == nSpreader)
	{
		if (nWorkType == PLCInfo::WORK_TYPE_IN)
		{
			if (nContPos == PLCInfo::CN_POS_F)
			{
				//20尺，收箱，前箱位作业
				int nTruckHeadDoorType = 0;
				for (auto& door : m_mapCamDoorRet)
				{
					auto iter = g_PositionCfg.m.find(door.first);
					if (iter != g_PositionCfg.m.end())
					{
						if (iter->second == CAM_POS_FRONT)
						{
							nTruckHeadDoorType = door.second.type;
							AddTaskLog(console->info("20尺收箱作业前箱，车头侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
						}
					}
				}
				if (nTruckHeadDoorType > 0 &&
					nTruckHeadDoorType != DoorResult::BACK_DOOR)
				{
					AddTaskLog(console->warn("20尺收箱作业前箱，方向正确，车头侧识别为非箱门"), true);
					return DOOR_DIR_CORRECT;
				}
				else
				{
					AddTaskLog(console->warn("20尺收箱作业前箱，方向错误，车头侧识别为箱门"), true);
					return DOOR_DIR_INCORRECT;
				}
			}
			else
			{
				//20尺，收箱，后箱位作业
				int nTruckTailDoorType = 0;
				for (auto& door : m_mapCamDoorRet)
				{
					auto iter = g_PositionCfg.m.find(door.first);
					if (iter != g_PositionCfg.m.end())
					{
						if (iter->second == CAM_POS_BACK)
						{
							nTruckTailDoorType = door.second.type;
							AddTaskLog(console->info("20尺收箱作业后箱，车尾侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
						}
					}
				}
				if (nTruckTailDoorType > 0 &&
					nTruckTailDoorType == DoorResult::BACK_DOOR)
				{
					AddTaskLog(console->warn("20尺收箱作业后箱，方向正确，车尾侧识别为箱门"), true);
					return DOOR_DIR_CORRECT;
				}
				else
				{
					AddTaskLog(console->warn("20尺收箱作业后箱，方向错误，车尾侧识别为非箱门"), true);
					return DOOR_DIR_INCORRECT;
				}
			}
		}
		else
		{
			//20尺发箱作业
			int nTruckHeadDoorType = 0;
			int nTruckTailDoorType = 0;
			for (auto& door : m_mapCamDoorRet)
			{
				auto iter = g_PositionCfg.m.find(door.first);
				if (iter != g_PositionCfg.m.end())
				{
					if (iter->second == CAM_POS_FRONT)
					{
						nTruckHeadDoorType = door.second.type;
						AddTaskLog(console->info("20尺发箱作业，车头侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
					}
					else if (iter->second == CAM_POS_BACK)
					{
						nTruckTailDoorType = door.second.type;
						AddTaskLog(console->info("20尺发箱作业，车尾侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
					}
				}
			}
			if (nTruckHeadDoorType != 0 &&
				nTruckTailDoorType != 0)
			{
				if (nTruckHeadDoorType == DoorResult::BACK_DOOR &&
					nTruckTailDoorType != DoorResult::BACK_DOOR)
				{
					AddTaskLog(console->warn("20尺发箱作业，方向错误，车头侧识别为箱门，车尾侧识别为非箱门"), true);
					return DOOR_DIR_INCORRECT;
				}
			}
			AddTaskLog(console->warn("20尺发箱作业，方向正确，不是“车头侧识别为箱门，车尾侧识别为非箱门”的情况"), true);
			return DOOR_DIR_CORRECT;
		}
	}
	else if (PLCInfo::SINGLE40 == nSpreader ||
		PLCInfo::SINGLE45 == nSpreader)
	{
		//40尺作业
		int nTruckHeadDoorType = 0;
		int nTruckTailDoorType = 0;
		for (auto& door : m_mapCamDoorRet)
		{
			auto iter = g_PositionCfg.m.find(door.first);
			if (iter != g_PositionCfg.m.end())
			{
				if (iter->second == CAM_POS_FRONT)
				{
					nTruckHeadDoorType = door.second.type;
					AddTaskLog(console->info("40尺作业，车头侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
				}
				else if (iter->second == CAM_POS_BACK)
				{
					nTruckTailDoorType = door.second.type;
					AddTaskLog(console->info("40尺作业，车尾侧 {} 识别为 conf={} type={}", door.first, door.second.conf, EnumTransform::GetDoorResult(door.second.type)), true);
				}
			}
		}
		if (nTruckHeadDoorType != 0 &&
			nTruckTailDoorType != 0)
		{
			if (nTruckHeadDoorType == DoorResult::BACK_DOOR &&
				nTruckTailDoorType != DoorResult::BACK_DOOR)
			{
				AddTaskLog(console->warn("40尺作业，方向错误，车头侧识别为箱门，车尾侧识别为非箱门"), true);
				return DOOR_DIR_INCORRECT;
			}
		}
		AddTaskLog(console->warn("40尺作业，方向正确，不是“车头侧识别为箱门，车尾侧识别为非箱门”的情况"), true);
		return DOOR_DIR_CORRECT;
	}
	return 0;
}

void TaskManager::UpdateDoorOpenRetToPlc()
{
	m_mtxTaskInfo.lock();

	if (m_mapCamOpenRet.size())
	{
		//识别结果中，有打开就认为是打开
		bool bHasOpenRet = false;
		bool bisOpenRet = false;
		int Hasback = 0;
		float conf=0;
		for (auto& t : m_mapCamOpenRet)
		{
			AddTaskLog(console->warn("箱门打开结果[{} type={}(1=CLOSE,2=OPEN),count={},valid_count={},back={},conf={}]",
				t.first, t.second.open_type, t.second.ret_count, g_AppConfig.userCfg.door_open_count, t.second.front_door, t.second.conf), true);
			if (t.second.ret_count < conf)
			{
				conf = t.second.ret_count;
			}
			bisOpenRet |= (t.second.open_type == DoorOPENResult::OPEN_TYPE::DoorOPENResult_OPEN_TYPE_OPEN);//识别结果中，有打开就认为是打开
			if (!t.second.front_door)
			{
				Hasback += 1;//判断识别结果中非前门数量
			}
			if (t.second.ret_count >= g_AppConfig.userCfg.door_open_count)
			{
				//检测到非前门，并且是打开状态才认为是箱门打开，减少误报
				if (t.second.open_type == DoorOPENResult::OPEN_TYPE::DoorOPENResult_OPEN_TYPE_OPEN &&
					!t.second.front_door)
				{
					bHasOpenRet = true;
					AddTaskLog(console->warn("判断为箱门打开[{}]", t.first), true);
				}
			}

		}
		if (Hasback ==0 )
		{
			bHasOpenRet = bisOpenRet;
			AddTaskLog(console->info("无后门识别结果，有打开则打开[{}]", bHasOpenRet), true);
		}

		m_bDoorOpenResult = bHasOpenRet;

		if (bHasOpenRet)
		{

			g_mtxOCRToPlc.lock();
			if (g_OCRToPlc.doorOpen != OCRToPlc::DOOR_OPEN)
			{
				g_OCRToPlc.doorOpen = OCRToPlc::DOOR_OPEN;
				string strLog = console->warn("更新箱门打开状态 OPEN {}", g_OCRToPlc.to_string());
				AddTaskLog(strLog);
				if (m_taskState == 1)
				{
					WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_SendToPlc.txt", strLog);
				}
			}
			g_mtxOCRToPlc.unlock();
		}
		else
		{
			g_mtxOCRToPlc.lock();
			if (g_OCRToPlc.doorOpen != OCRToPlc::DOOR_CLOSE)
			{
				g_OCRToPlc.doorOpen = OCRToPlc::DOOR_CLOSE;
				string strLog = console->warn("更新箱门打开状态 CLOSE {}", g_OCRToPlc.to_string());
				AddTaskLog(strLog);
				if (m_taskState == 1)
				{
					WriteStringToTxt(m_TaskInfo.local_snapshot_location() + GetSysTimeString() + "_SendToPlc.txt", strLog);
				}
			}
			g_mtxOCRToPlc.unlock();
		}
		if (Sany_Code.open_code != DoorOpen_TimeoutOffline && Sany_Code.open_code != DoorOpen_TimeoutOnline)
		{
			if (conf < 0.8)
			{
				Sany_Code.open_code = DoorOpen_LowConfidence;
			}
			else
			{
				Sany_Code.open_code = DoorOpen_Normal;
			}

		}
	}
	else
	{
		g_mtxOCRToPlc.lock();
		g_OCRToPlc.doorOpen = OCRToPlc::DOOR_NONE;
		g_mtxOCRToPlc.unlock();
	}

	m_mtxTaskInfo.unlock();
}

void TaskManager::SaveCSV(const string& log_str)
{

	//string savePath = g_AppConfig.userCfg.savePath + "/" + GetDateString() + "/" + GetDateString() + ".csv";
	//string savePath = g_AppConfig.userCfg.savePath + "/" + to_string(i) + "/" + GetDateString() + ".csv";

	string savePath1 = g_AppConfig.userCfg.savePath + "/" + GetDateString();
	string savePath = savePath1 +  "/" + GetDateString() + ".csv";
	MakeDirs(savePath1);

	if (FILE *file = fopen(savePath.c_str(), "r"))
	{
		fclose(file);
	}
	else
	{
		WriteStringToTxt(savePath, "设备号,任务号,任务类型,开始时间,TOS车号,车号识别结果,车号是否成功,集卡引导超时,车号发送时间,TOS箱号,箱号识别结果,箱号是否成功,箱号发送时间,结束时间,车号照片路径,箱号照片路径,备注\n");
	}

	WriteStringToTxt(savePath, log_str);
}
