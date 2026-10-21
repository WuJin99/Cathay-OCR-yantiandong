
#pragma once
#include <queue>
#include "Devices.h"
#include "SyncQueue.h"
#include "Utils.h"
#include "PLCInfo.h"
#include "ThreadQue.hpp"

class TaskManager;
class CDHCam : public DHIPC
{
public:
	int m_lane;
	LicPlateResult m_LicPlateResult;
	//保存矫正的图片
	cv::Mat rectifyDstMat;
	std::string m_imagePath;
	// 线程退出标志
	bool m_exitFlag;

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
	std::map<std::string, const Event *> m_EvenList;

	// 配置listener监听的事件
	void SetupListenerEvents();

	//mat转JPEGPic
	void Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic);

	// 执行时间动作
	int Action(const std::string &eventName, const Event *event);

	// 设定云台角度
	int SetPTZ(const PTZ& pos);
	struct VideoItem
	{
		string localPath;
		string recodeFullPath;
		string recodeSinglePath;
		string ftpFullPath;
		string ftpSinglePath;
		VideoItem()
		{
			localPath = "";
			recodeFullPath = "";
			recodeSinglePath = "";
			ftpFullPath = "";
			ftpSinglePath = "";
		}
		void Clear()
		{
			localPath = "";
			recodeFullPath = "";
			recodeSinglePath = "";
			ftpFullPath = "";
			ftpSinglePath = "";
		}
	};
	struct tEvent
	{
		Event *event;
		std::string eventName;
	};
	VideoItem m_videoItem;
	ThreadQueue<VideoItem> m_queueVideoCodec;
	ThreadQueue<tEvent> m_queueCap;
	ThreadQueue<int> m_queueSetPos;
	struct tCapImage
	{
		std::shared_ptr<JPEGPic> pJPEG;
		Event *event;
		std::string eventName;
		//位置信息 PTZ
		int P;
		int T;
		int Z;
	};
	ThreadQueue<tCapImage> m_queueCapImage;
	//struct VideoSavePath
	//{
	//	string ori_path;
	//	string ftp_path;
	//};
	//ThreadQueue<string> m_queueVideoCodec;
	std::atomic<long long> m_llMsPtz;//发生PTZ时的毫秒数
	std::atomic<long long> m_llMsCap;//发生CAP时的毫秒数
	Event *m_pCapEvent;
	std::string m_strCapEventName;
	std::string m_workId;
	void DealSetEvent(std::string eventName, Event *event);
	void DealCapEvent(std::string eventName, Event *event);
	void DealStepEvent(std::string eventName, Event *event);
	void DealImage(tCapImage&cap_image);
	void DoCapture(std::string eventName, Event *event);
	void CheckPtzDone();

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


	CDHCam(std::string id, Config *config, TaskManager*pMgr);

	virtual ~CDHCam();

	// 启动线程
	int StartThreads();

	void StopThreads();

	TaskManager *m_pTaskMgr;
	//开始车牌检测
	bool StartPlateDetection();
private:


	GRpc_Image::Img_type GetImgType();

	void MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName);

	void WriteEventInfoToLocal(const string &path, const string &src);//记录事件的触发记录到本地
};
