#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "Utils.h"
#include "PLCInfo.h"
#include "CameraConfig.h"
#include "SyncQueue.h"
#include "ocrs.pb.h"
#include "ZmqComm.h"

// #include "CGRPCRequest.h"
#include "ComposeResult.h"
#include "RecordPLCData.h"
#include "ThreadQue.hpp"
#include "DeviceManager.h"
#include "json.hpp"
using json = nlohmann::json;
class ComposeResult;
class PicSave;
using namespace ocrs::pb;

//将识别结果保存到队列中，防止处理图片时间长导致调用CRS慢
struct tCrsRet
{
	int img_label;
	string img_type;
	std::shared_ptr<GRpc_RepeadResult> pGRpcRet;
};

class TaskManager
{
public:
	// 处理事件函数
    void TaskTrigger(const PLCInfo &plcInfo);

    void SerialSingleRTaskProto(RRIMAGELIST &rImageList);

    TaskManager();

	~TaskManager();

    void Start(string side, string laneid, string cfg_file);

    void Stop();

    void SetTaskID();

	void SendResult(bool isSendReleaseTruck = false);

	std::atomic<bool> m_bDoorOpenReNeed;
	std::atomic<bool> m_bDoorDirReNeed;
	std::atomic<bool> m_bDoorOpenResult;
	//获取箱门方向是否正确，1=正确，2=错误
	int GetDoorDirCorrect();
private:
	string ShowEvents(bool b=true);
	bool IsHaveEvent(int e);
	bool InitTaskData();
	bool UnInitTaskData();
	bool IsLaneIn();//判断是否车道内
	bool IsContinueTaskEnd(const int &eventType); //判断异常重启作业是否结束

	//集卡停到位
	void DealEventTruckReady();
	//集卡离开
	void DealEventTruckLeave(int lane);
	//吊具着箱
	void DealEventLand();
	//吊具非着箱
	void DealEventUnLand();
	//发生闭锁
	void DealEventLock();
	//发生开锁
	void DealEventUnLock();
	//空吊具起升
	void DealEventHoistUp();
	//空吊具下降
	void DealEventHoistDown();
	//空吊具移动
	//void DealEventHoistMove();
	//带箱起升
	void DealEventContUp();
	//带箱下降
	void DealEventContDown();
	//带箱移动
	void DealEventContMove();
	//作业结束或丢弃
	void DealEventTaskEnd();
protected:
	int64_t taskPreContinueTime = 0;
	boost::mutex taskMutex;
private:

	bool m_isSavePlcInfo;
	int  m_eventType;

    std::thread worker;  // 工作线程

    std::thread listener;  // 工作线程

	bool exitFlag;
	bool m_isContinueTask; //异常重启的作业是否进行超时处理；
    PLCInfoState m_plcInfoState;
	int m_lockGantryValue;
	int m_lockTrallyValue;
	int m_lockHoistValue;

    string m_spreader;	            // 吊具类型

	//std::string m_taskSavePath;			//保存作业信息路径，用来重启恢复时获取历史作业

	set <int> m_eventSet;			//保存已经发生的事件  装船事件<=10，卸船>10和<=20

	// 车道配置
	LaneConfig m_LaneCfg;
	//相机位置配置
	PositionConfig m_posCfg;
	// 作业任务配置
	TaskEventConfig m_TaskEventCfg;
	std::shared_ptr<ComposeResult> m_pComposeResult;
	std::shared_ptr<CDataIntegration> m_pDataIntegration;

	struct tTaskLog
	{
		string time;
		string path;
		string info;
		tTaskLog(TaskInfo&task_info)
		{
			SYSTEMTIME sysTime;
			GetLocalTime(&sysTime);
			char szTime[100] = {};
			sprintf_s(szTime, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
				sysTime.wYear, sysTime.wMonth, sysTime.wDay,
				sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
			time = szTime;
			path = task_info.local_snapshot_location() + task_info.work_id() + "_flow.txt";
		}
	};
	SyncQueue<shared_ptr<tTaskLog>> m_queueLog;
	//保存车牌识别结果，vehPic中无数据
	vector<LicPlateResult> m_arrPlateHead;//车牌号
	vector<LicPlateResult> m_arrPlateTail;//车架号

	int m_nLockType;
	atomic<bool> m_bRecvJobTrigger;//是否接收到JobTrigger，只有接收了并且开锁时才认为作业结束，创建新的taskid
	atomic<bool> m_bRecvJobTriggerSendLog;//是否接收到JobTrigger，并打印一条消息
	std::atomic<int> m_nUpdateResult;
	std::atomic<bool> m_bIsSendingResult;
public:
	enum
	{
		//m_taskState
		TASKNONE = 0,
		TASKSTART = 1,
		TASKEND = 2,
		//m_nLockType
		LOCK_NONE = 0,//
		LOCK_IN_LANE, //在车道内闭锁
		LOCK_OUT_LANE,//在车道外闭锁
		//箱门方向结果,GetDoorDirCorrect()
		DOOR_DIR_CORRECT = 1,  //箱门方向正确
		DOOR_DIR_INCORRECT = 2,//箱门方向错误
	};
	//三一要求故障码不能重复，按车号、箱号、箱门方向、箱门打开递增，详见《SanyPIIS-OCR Api-20260617.docx》。
	enum CN_Code {
		ContainerNo_Normal = 0,
		// 箱号识别异常
		ContainerNo_TimeoutOnline = 8,  // 程序在线，超时未返回
		ContainerNo_TimeoutOffline = 9,  // 程序离线，超时未返回
		ContainerNo_Mismatch = 10,  // 识别结果与任务派发箱号不一致
		ContainerNo_LowConfidence = 11,  // 识别结果置信度低
		// ISO识别异常
		ISO_TimeoutOnline = 12,  // 程序在线，超时未返回
		ISO_TimeoutOffline = 13,  // 程序离线，超时未返回
		ISO_LowConfidence = 14   // 识别结果置信度低
	};
	// 车号 StatusCode（PIIS get-pmn-data）
	// 识别正常->0
	// 内集卡：1/4/5；无车顶号（含无车）→4；CRS 2/3 无号时保持；有车顶号可覆盖 2/3
	// 外集卡：6/7; 车牌一致→0；无牌车→6；识别错误或为空→7（不再因无车保持默认 0）
	enum Truck_Code {
		// 正常状态（内集卡：车顶号与任务一致；外集卡：车牌与任务一致，均回 0）
		Normal_FromPmn = 0, // 识别正确，车号结果来自车顶号或外集卡车牌
		Normal_FromQR = 1,  // 识别正确，车顶号结果来自二维码

		// 车顶号识别异常
		TopCode_TimeoutOnline = 2,   // CRS 在线，发图后在 CRS_GRPC_WaitTime 内超时无回包（SerialSingleRTaskProto）；流识别有号后可被 GetPmnResult 覆盖
		TopCode_TimeoutOffline = 3,  // GetOcrResult 失败且非 DEADLINE_EXCEEDED（stub 未连、RPC 错误等）；流识别有号后可被 GetPmnResult 覆盖
		TopCode_Mismatch_NeedChangeTruck = 4,  // 车顶号与任务不一致，或无车顶号空结果（非 2/3 时，含车道无车）
		TopCode_LowConfidence = 5,  // 车顶号识别异常，车顶号识别结果置信度低

		// 车牌号识别异常（外集卡）
		Plate_NoResult = 6,  // 识别结果为「无牌车」
		Plate_Mismatch = 7   // 车牌识别错误（与任务不一致），或识别为空/无有效车牌
	};
	enum DoorDir_Code {
		DoorDir_Normal = 0,
		DoorDir_TimeoutOnline = 15,   // 程序在线，超时未返回
		DoorDir_TimeoutOffline = 16,  // 程序离线，超时未返回
		DoorDir_LowConfidence = 17    // 识别结果置信度低
	};
	enum DoorOpen_Code {
		DoorOpen_Normal = 0,
		DoorOpen_TimeoutOnline = 18,   // 程序在线，超时未返回
		DoorOpen_TimeoutOffline = 19,  // 程序离线，超时未返回
		DoorOpen_LowConfidence = 20    // 识别结果置信度低
	};
	struct Err_Code
	{
		int cn_code;
		int truck_code;
		int door_code;
		int open_code;
	}Sany_Code; //上报各识别数据识别异常

	int m_taskState;
	PLCInfo m_plcinfo;
	mutex m_plcinfoMtx;

	// 车号结果
	struct PmnResult
	{
		int type;
		string qr;
		string pmn;
		string path;
		float conf;
		int GantryValue;
		int nCount =0;
		bool fromQr = false; // 车顶号是否来自二维码（StatusCode=1）
		void Clear()
		{
			type = 0;
			qr ="";
			pmn = "";
			conf = 0.0;
			path = "";
			GantryValue = 0;
			nCount = 0;
			fromQr = false;
		}
		std::string to_string()
		{
			return fmt::format("type:{}, qr:{}, pmn:{}, conf:{}, nCount:{}, GantryValue:{}, fromQr:{}, path:{}",
				type, qr, pmn, conf, nCount, GantryValue, fromQr, path);
		}
	};
	struct PmnPre
	{
		int spreaderMode;
		string pmn;
	};

	atomic<bool> m_IARSPmn;//采集程序主动抓拍识别车号结果
	atomic<bool> m_CRSPmn;//识别程序流识别车号结果
	atomic<bool> m_TruckNumResultNeed;//车号请求结果
	atomic<bool> m_CNResultNeed;//车号请求结果
	atomic<bool> m_CNResultSend;//车号请求结果是否已经发送
	atomic<bool> m_TruckResultSend;//车号请求结果是否已经发送
	int64_t m_StartSendPmnTime;//要结果的时间
	PmnResult m_IARSInnerPmn;//采集程序主动识别的内集卡方向的车号结果
	PmnResult m_IARSOuterPmn;//采集程序主动识别的外集卡方向的车号结果
	PmnResult m_CRSInnerPmn;//识别程序主动识别的内集卡方向的车号结果
	PmnResult m_CRSOuterPmn;//识别程序主动识别的外集卡方向的车号结果
	PmnPre m_prePmn;
	int m_truckNumSuccess;//车号比对结果
	int m_cnNumSuccess;//车号比对结果
	int m_truckType;//作业集卡类型
	int m_spreaderMode;//当前作业吊具类型

	string m_strJobPath;
	int64_t m_sendTruckNumTime;//发送车号结果的时间，结果发送太快汇川处理会有问题，所以发送结果稍慢一些
	std::shared_ptr<CDeviceManager> m_pDevMgr;
	string m_plateNum;//用来保存最后识别到的车牌结果
	string m_TruckNum;//用来保存最后识别到的车号结果
	string m_planTruckNum; //计划作业车号
	atomic<bool> m_planTruckMatch;//跟计划车号是否匹配
	atomic<bool> m_planCnMatch;//跟计划箱号是否匹配
	OcrData m_OcrData;
	TaskInfo m_TaskInfo;
	JobTrigger m_JobTrigger;
	std::mutex m_mtxTaskInfo;
	std::map<int/*车道*/,std::vector<tCnItem>> m_arrCnItem;

	// 车顶号样本（每识别一条，拉号时再按 PIIS 请求时 gantry 窗口聚合）
	std::map<int/*车道*/, std::vector<PmnResult>> m_arrPmnItem;
	std::map<int/*车道*/, PmnResult> m_arrPmnItem2; //外卡车牌
	std::mutex m_mtxCnItem;
	std::mutex m_mtxPmnItem;
	ThreadQueue<RRIMAGELIST> m_RImageList;
	string m_task_id;	                // 任务号唯一
	string m_lane_id;			        // 车道ID，为0表示堆场内，否则为作业车道
	string m_task_dir;                  // 任务路径（日期\\任务号,不包含配置文件根路径）
	int m_reqPmnCount;   //请求车号次数
	//本地记录作业数据及与tos的比对情况
	struct tRetInfo
	{
		string jobid;
		string quayNo;
		string taskType;
		string startTime;
		string tosTruck;
		string truck;
		string truckSucceed;
		string cpsTimeout;
		string truckTime;
		string tosCn;
		string cn;
		string cnSucceed;
		string cnTime;
		string endTime;
		string pmnPath;
		string cnPath;
		vector<string> vecComment;

		tRetInfo()
		{
			Clear();
		}

		void Clear()
		{
			jobid = "";
			quayNo = "";
			taskType = "";
			startTime = "";
			tosTruck = "";
			truck = "";
			truckSucceed = "";
			cpsTimeout = "";
			truckTime = "";
			tosCn = "";
			cn = "";
			cnSucceed = "";
			cnTime = "";
			endTime = "";
			pmnPath = "";
			cnPath = "";
			vecComment.clear();
		}

		string to_string()
		{
			string ret;
			ret.append(quayNo);
			ret.append(",");
			ret.append(jobid);
			ret.append(",");
			ret.append(taskType);
			ret.append(",");
			ret.append(startTime);
			ret.append(",");
			ret.append(tosTruck);
			ret.append(",");
			ret.append(truck);
			ret.append(",");
			ret.append(truckSucceed);
			ret.append(",");
			ret.append(cpsTimeout);
			ret.append(",");
			ret.append(truckTime);
			ret.append(",");
			ret.append(tosCn);
			ret.append(",");
			ret.append(cn);
			ret.append(",");
			ret.append(cnSucceed);
			ret.append(",");
			ret.append(cnTime);
			ret.append(",");
			ret.append(endTime);
			ret.append(",");
			ret.append(pmnPath);
			ret.append(",");
			ret.append(cnPath);
			ret.append(",");
			for (auto item : vecComment)
			{
				ret.append("[");
				ret.append(item);
				ret.append("]");
			}
			ret.append("\n");
			return ret;
		}
	};

	tRetInfo m_tRetInfo;

	LicPlateResult m_PlateResult;//外集卡车牌结果
	PlateMatch m_TruckNoPlateNum;//车号和车牌号
	struct tDoorOpenRet
	{
		int open_type;//箱门打开识别结果，1=CLOSE,2=OPEN
		int ret_count;//结果累计计数
		bool front_door;//是否为前门，只有是非前门才可能为开
		float conf;//置信度
		tDoorOpenRet()
		{
			conf = 0;
			front_door = false;
			open_type = 0;
			ret_count = 0;
		}
	};
	//key:cam_name, val:箱门打开结果
	map<string, tDoorOpenRet> m_mapCamOpenRet;
	struct tDoorRet
	{
		int type;//箱门方向
		float conf;//置信度
		tDoorRet()
		{
			conf = 0;
			type = 0;
		}
	};
	//key:cam_name
	map<string, tDoorRet> m_mapCamDoorRet;
	std::mutex m_mtxCamDoorRet;

	int cur_truckStatus[3];  // 1：停止 2：离开，3：移动
	int old_truckStatus[3];
	std::mutex m_truckNumDequeMutex;
	std::vector <TruckNumSend> m_truckNumDeque;
	std::map <string,TruckNumInfo> m_truckNumInfoMap;
	void SendTaskInfo(EventInfo::EVENT_TYPE type, std::string event_name, int gantry_pos, int hoist_pos, int trolly_pos, int isLaneIn = 2, int needReply = 0);
	void ComposeCnRet(std::vector<tCnItem> cnArr, tCnItem& cn_item);
	void AddTaskLog(const string& info, bool bMainLog=false);
	//写入TaskLog日志到文件
	void SaveLogToFile();
	//设置车牌识别结果，ICEGun接收到车牌结果时调用
	void SetPlateResult(LicPlateResult& plate_result, shared_ptr<cv::Mat> pImg);
	//设置内集卡车顶号结果
	void SetTruckResult(ocrs::pb::THPMResult& truck_result);
	//设置集卡状态 1表示集卡车停止；2表示集卡车离开；3表示集卡车移动
	void SetTruckStatus(ocrs::pb::TRUCKSTATUS& truck_ststus);
	void SetJobTrigger(gui::pb::JobTrigger& trigger);
	//
	void SetDoorOpenRet(int img_label, DoorOPENResult& dor);
	void SetDoorDirRet(int img_label, DoorResult& dr);
	void UpdateDoorOpenRetToPlc();
	string ComposePmnResult(int truckType, int spreaderMode, bool &bMatch);
	bool SimilarMatch();
	bool SimilarMatchOnce(PmnResult &pmn, const string &baseStr);
	bool SimilarMatchPmn(const string &baseStr);
	void GetCnResult(string& json_str);
	/** 吊具到箱顶距离向上经过 hoistToContUpVal 时：清空箱号候选与箱号图片缓存（不删本地文件），便于再次下降重拍识别 */
	void ClearCnResultOnHoistToContUp();

	/** PIIS 拉取车顶号/车牌 JSON：按请求瞬间 gantry 做窗口，|样本gantry-请求gantry|≤Delta 的样本按号累加后再选；不要求大车停稳 */
	void GetPmnResult(string& json_str);
	void SaveCSV(const string& log_str);
	/** 追加一条样本（pmn + 单帧conf + 当时gantry），不按号合并累加；fromQr 表示来自二维码 */
	PmnResult* AddPmnRet(int lane, string pmn, float conf, bool fromQr = false);

	/**
	 * 车顶号流识别入库门控。
	 * 行驶中允许入池（记样本）。启用时仅校验车道有效 + hasTruck。
	 * 清号由 hasTruck 消抖后 ClearPmnCache 完成，不再走视觉 DealEventTruckLeave。
	 * 配置：AppConfig USER pmnWindow*
	 */
	bool IsPmnCaptureWindow(int lane, string* pFailReason = nullptr) const;
	/**
	 * 清指定车道车顶号样本与车牌。仅清缓存，不触发视觉 DealEventTruckLeave。
	 */
	void ClearPmnCache(int lane, const string& reason);
	/**
	 * 跟踪各车道入库门控；有车→无车消抖达标后清车顶号与车牌。大车移动不清池。
	 * 有车时尝试将暂存车牌写入主池。
	 */
	void UpdatePmnWindowState();

private:
	/** 无车时暂存的车牌（未入 m_arrPmnItem2），有车或超时后处理 */
	struct PendingPlate
	{
		bool valid = false;
		string pmn;
		float conf = 0.f;
		string path;
		int64_t tickMs = 0;
		int gantry = 0;
		void Clear()
		{
			valid = false;
			pmn.clear();
			conf = 0.f;
			path.clear();
			tickMs = 0;
			gantry = 0;
		}
	};
	static int64_t PmnSteadyMs();
	void SavePendingPlate(int lane, const string& pmn, float conf, const string& path, int gantry);
	void ClearPendingPlate(int lane);
	/** 已持有 m_mtxPmnItem：未过期 pending 写入 m_arrPmnItem2，过期则丢弃 */
	void CommitPendingPlateUnderLock(int lane);
	/** 加锁后调用 CommitPendingPlateUnderLock */
	void TryCommitPendingPlate(int lane);
	void DropExpiredPendingPlate(int lane);
	/** 窗内同一车顶号的临时汇总（只用于本次拉号选优，不回写样本池） */
	struct PmnSet
	{
		float conf = 0.f;      // 窗内该号各样本单帧 conf 之和
		int nCount = 0;        // 窗内该号样本条数
		int lastGantry = 0;    // 窗内该号最后一条样本的 gantry
		string path;           // 窗内该号最后一条非空图片路径
		bool fromQr = false;   // 窗内该号是否出现过来自二维码的样本
	};

	/** 窗内按 pmn 聚合样本，不回写样本池 */
	static std::map<string, PmnSet> AggregatePmnInWindow(
		const std::vector<PmnResult>& samples,
		int reqGantry,
		int gantryDelta,
		int* outInWindow = nullptr);

	/** 选号策略：nCount 优先，同票比 conf 之和 */
	static bool SelectBestPmn(
		const std::map<string, PmnSet>& winSet,
		string& outPmn,
		PmnSet& outSet);

	static void FillPmnResultFromSet(
		const string& pmn,
		const PmnSet& set,
		PmnResult& out);

	/** 车牌缓存是否在 PIIS 拉号 gantry 窗口内（单槽，复用 pmnWindowEnable/GantryDelta） */
	static bool IsPlateInSendWindow(const PmnResult& plate, int reqGantry, int gantryDelta);

	void UpdatePlateResult();   // 更新车牌结果，发送时按 reqGantry 窗口过滤
	void ActionCameraByTrigger(gui::pb::JobTrigger& trigger);
	SyncQueue<tCrsRet> m_CrsRetList;

	bool m_pmnWindowInLane[3];   // 逻辑门控状态(带迟滞)，消抖期间可为 true；下标=车道号
	int m_hasTruckZeroFrames[3]; // 门控内 hasTruck 连续为 0 的帧计数，达 pmnNoTruckClearFrames 才清池
	int m_gantryAnchor;          // 进入门控时记录的大车编码器，仅诊断日志
	PendingPlate m_pendingPlate[3]; // 无车早识别暂存，下标=车道号

	};
#endif //TASKMANAGER_H
