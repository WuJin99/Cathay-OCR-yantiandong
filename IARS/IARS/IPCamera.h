#pragma once

#include "Utils.h"
#include "opencv2/highgui.hpp"
#include "Devices.h"
#include "PLCInfo.h"
#include "ThreadQue.hpp"
using namespace std;
using namespace cv;

class TaskManager;
class IPCamera :public CDevice
{
public:
	IPCamera(std::string id, Config *config, TaskManager*pMgr);
	virtual ~IPCamera();

	virtual bool LogIn();
	// 设备登出
	virtual void LogOut();
	// 是否已登录
	virtual bool HasLogIn();
	// 是否在取流
	virtual bool IsStreaming();
	// 是否在解码
	virtual bool IsDecoding();
	// 开始取流
	virtual bool StartStreaming();
	// 停止取流
	virtual void StopStreaming();
	// 开始解码
	virtual bool StartDecoding();
	// 停止解码
	virtual void StopDecoding();
	// 设备运行状态检查
	virtual void RemoteControl();

	// 返回 IP 地址
	virtual std::string GetDeviceIP();

	// 返回 安装位置
	virtual std::string GetInstallNmae();

	// 抓拍图片 码流抓图
	virtual bool Capture(Mat& mat);

	// 抓拍图片 设备抓图
	virtual bool CaptureNEW(Mat& mat);
public:
	// 启动线程
	int StartThreads();

	void StopThreads();
	bool isOpened();
protected:
	// 执行时间动作
	int Action(const std::string &eventName, const Event *event);
private:
	void SetupListenerEvents();
	void Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic);
public:
	
	TaskManager *m_pTaskMgr;
	HKGunConfig * m_pGunCfg;
private:
	// 线程退出标志
	bool m_exitFlag;
	bool m_cnFirst;
	int m_lane;
	mutex m_matMutex;
	VideoCapture m_cap;
	std::string m_strIp;
	std::string m_strCamName;
	std::string m_strCamId;
	string m_rtspUrl;
	// PLC信息
	PLCInfo m_plcInfo;

	// PLC状态
	PLCInfoState m_plcInfoState;
	// 事件名称与事件一一对应
	std::map<std::string, const Event *> m_EvenList;

	std::atomic<time_t> m_tLastReadData;

	ThreadQueue<tCapImage> m_queueCapImage;
	void DealImage(tCapImage& cap_image);
	void MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName);
};

