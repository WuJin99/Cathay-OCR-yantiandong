// 海康枪机
#ifndef HKGUN_H
#define HKGUN_H
#include <queue>
#include "Devices.h"
#include "SyncQueue.h"
#include "Utils.h"
#include "PLCInfo.h"
#include "ThreadQue.hpp"
class TaskManager;
class CDeviceManager;
class CHKGun : public CHKIPC
{
public:

	
	std::string m_imagePath;
    // 线程退出标志
    bool m_exitFlag;

	int m_lane;
    // 执行动作线程
    std::thread m_worker;

    // 执行动作线程
    std::thread m_reader;

	// 图像采集线程
	std::thread m_cap;

    // PLC信息
    PLCInfo m_plcInfo;

    // PLC状态
    PLCInfoState m_plcInfoState;

    // PLC 数据更新条件变量
    boost::condition_variable m_EventCond;

    // PLC 数据更新条件锁
    boost::mutex m_EventMutex;    
    
    // 事件名称与事件一一对应
    std::map<std::string,const Event *> m_EvenList;

    // 配置listener监听的事件
    void SetupListenerEvents();

	//mat转JPEGPic
	void Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic);

    // 执行时间动作
    int Action(const std::string &eventName,const Event *event);

    // 设定云台角度
	int SetPTZ(const PTZ& pos);

	struct tEvent
	{
		Event *event;
		std::string eventName;
	};
	ThreadQueue<tEvent> m_queueCap;
	ThreadQueue<tCapImage> m_queueCapImage;
	//struct VideoSavePath
	//{
	//	string ori_path;
	//	string ftp_path;
	//};
	ThreadQueue<string> m_queueVideoCodec;
	std::atomic<long long> m_llMsPtz;//发生PTZ时的毫秒数
	std::atomic<long long> m_llMsCap;//发生CAP时的毫秒数
	Event *m_pCapEvent;
	std::string m_strCapEventName;

	void DealSetEvent(std::string eventName, Event *event);
	void DealCapEvent(std::string eventName, Event *event);
	void DealStepEvent(std::string eventName, Event *event);
	void DealTriggerEvent(std::string eventName, Event *event);
	void DealImage(tCapImage&cap_image);
	void DoCapture(std::string eventName, Event *event);
	void CheckPtzDone();
	void MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName);

	long long GetSysMillSec();
	boost::mutex m_Mp4PathMutex;
	std::atomic<long long> m_llMsSave;//开始保存视频时的毫秒数
	std::atomic<bool> m_bSaveData;//当前是否保存视频
	std::string m_save_path; //
	std::string m_singleMp4Path;//video视频文件名
	std::string m_strAutoCapEvent;//跳转到位后自动拍摄事件名称
	std::atomic<bool> m_bSetFinish;//SET是否完成
	std::atomic<bool> m_bCapFinish;//CAP是否完成
	std::atomic<bool> m_bStepFinish;//STEP是否完成，执行STEP时触发SET和CAP将忽略
	std::atomic<bool> m_bAutoCap;//跳转到位后自动拍摄
	//记录跳转过程中上一个PTZ位置
	std::atomic<int> m_nLastP;
	std::atomic<int> m_nLastT;
	std::atomic<int> m_nLastZ;
	std::queue<tEvent> m_queueStepEvent;
	
	enum
	{
		STEP_NONE = 0,
		STEP_SET,
		STEP_CAP,
	};
public:
	TaskManager *m_pTaskMgr;
	HKGunConfig * m_pGunCfg;

    CHKGun(std::string id, Config *config, TaskManager*pMgr);

    virtual ~CHKGun();

    // 启动线程
    int StartThreads();

    void StopThreads();

	//保存视频文件
	void SaveRealData(bool bOpen);
private:
	GRpc_Image::Img_type GetImgType();

	void WriteEventInfoToLocal(const string &path, const string &src);//记录事件的触发记录到本地
};

//海康车牌相机
class CHKPlate : public CHKIPC
{
public:
	// 线程退出标志
	bool m_exitFlag;
	int  m_lane;
	LONG m_lHandle;		// 启用布防
public:
	TaskManager *m_pTaskMgr;
	HKPlateConfig * m_pGunCfg;
	CDeviceManager * m_pDevMgr;
	LicPlateResult m_LicPlateResult;

	CHKPlate(std::string id, Config *config, TaskManager*pMgr);

	virtual ~CHKPlate();

	// 启动线程
	int StartThreads();

	void StopThreads();
	string GetType(int type);
	static void MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pThis);
};

#endif