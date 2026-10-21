#ifndef DEVICES_H
#define DEVICES_H

#include "Utils.h"
#include "CameraConfig.h"
#include <mutex>

#include "dhnetsdk.h"


class CDevice
{
public:
	std::atomic<bool> m_getCnResult;// 获取箱号结果
	std::atomic<bool> m_getPmnResult;//获取车号结果
    enum Status { LOGGEDOUT, LOGGEDIN, STREAMMING, DECODING, };
    enum CmdID { DEVICE_LOGIN, DEVICE_STREAMING_START, DEVICE_VIDEO_SHOW, DEVICE_CAPTURE, DEVICE_PTZ };
	//运行状态
	StateInfo::RUN_STATE stateType;
	StateInfo::RUN_STATE oldStateType;//保存上次状态，用来状态发生变化时，发送消息

protected:
    // 设备状态
    enum Status m_status;
    // 设备参数配置
    const Config *m_devCfg;

	std::mutex stateTypeMutex;

    // 设备类型
    std::string camID;
	// 保存触发的事件信息的txt路径
	//std::string eventInfoPath;

public:
	CDevice(std::string id, Config *config = NULL);
    virtual ~CDevice(){ }
    // 设备登录
    virtual bool LogIn(){ return false; }
    // 设备登出
    virtual void LogOut(){ return; }
    // 是否已登录
    virtual bool HasLogIn(){ return m_status >= LOGGEDIN; }
    // 是否在取流
    virtual bool IsStreaming(){ return m_status >= STREAMMING; }
    // 是否在解码
    virtual bool IsDecoding(){ return m_status >= DECODING; }
    // 开始取流
    virtual bool StartStreaming(){ return false; }
    // 停止取流
    virtual void StopStreaming(){ return; }
    // 开始解码
    virtual bool StartDecoding(){ return false; }
    // 停止解码
    virtual void StopDecoding(){ return; }
    // 设备运行状态检查
    virtual void RemoteControl(){ return; };
    
    // 返回 IP 地址
    virtual std::string GetDeviceIP(){ return""; }

    // 返回 安装位置
    virtual std::string GetInstallNmae(){ return""; }

    // 抓拍图片 码流抓图
    virtual bool Capture(JPEGPic& capPic);

    // 抓拍图片 设备抓图
    virtual bool CaptureNEW(JPEGPic& capPic);

    // 操作设备
    void UpdateStatus(Status s);

    bool OnCmdMsg(int nID);

	void SetStateType(StateInfo::RUN_STATE type);

};

// 海康SDK通用接口
class CHKIPC :public CDevice
{
protected:
    // 相机抓拍图片压缩参数
    NET_DVR_JPEGPARA jpegParam;

    // 已登录用户ID
    LONG m_loginUserID;

    // 通道号
    LONG m_lChannel;

    // 流预览句柄
    LONG m_lRealPlayHandle;

    // 播放 Port
    LONG m_lPort;

    // 是否解码
    bool m_bDecoding;

    // 播放窗体
    HWND m_hPlayWnd;

public:
    // 登录
    virtual bool LogIn();

    // 登出
    virtual void LogOut();

    // 返回 IP 地址
    std::string GetDeviceIP();
	bool GetCnFirst();

    // 返回 安装位置 
    std::string GetInstallNmae();

    // 设备运行状态检查
    virtual void RemoteControl();

	LONG GetLoginUserID();

protected:
    // 开始取流
    bool StartRealPlay(REALDATACALLBACK callback, void *userData = NULL/*, HWND hPlayWnd = 0*/);
    // 停止取流
    void StopRealPlay();
public:
    CHKIPC(std::string id, Config *config = NULL, int channel = 1);
    virtual ~CHKIPC();
    // 取流
    virtual bool StartStreaming();
    // 停止取流
    virtual void StopStreaming();
    // 开始解码
    virtual bool StartDecoding(/*HWND hPlayWnd*/);
    // 停止解码
    virtual void StopDecoding();
    // 抓拍图片
    virtual bool Capture(JPEGPic& capPic);
    // 抓拍图片 设备抓图
    virtual bool CaptureNEW(JPEGPic& capPic);
    // 设置云台角度: 0 - 成功; -1 - 失败
    virtual int Set_DVR_Config(NET_DVR_PTZPOS& pos);
    // 获取云台角度: 0 - 成功; -1 - 失败
    virtual int Get_DVR_Config(NET_DVR_PTZPOS& pos);
    // 云台控制操作: 0 - 成功; -1 - 失败(需要启动预览)
    virtual int Set_DVR_PTZControl(int dwPTZCommand, int dwStop);
    // 云台控制操作: 0 - 成功; -1 - 失败(无需启动预览)
    virtual int Set_DVR_PTZControl_Other(int dwPTZCommand, int dwStop);
};
//、、大华
class DHIPC :public CDevice
{
protected:

	// 已登录用户ID
	LLONG m_loginUserID;
	LONG m_plateHandle;
	// 通道号
	int m_lChannel;

	// 流预览句柄
	LLONG  m_lRealPlayHandle;

	// 播放 Port
	LONG m_lPort;

	// 是否解码
	bool m_bDecoding;

	// 播放窗体
	HWND m_hPlayWnd;

public:
	// 登录
	virtual bool LogIn();

	// 登出
	virtual void LogOut();

	// 返回 IP 地址
	std::string GetDeviceIP();



	// 返回 安装位置 
	std::string GetInstallNmae();

	// 设备运行状态检查
	virtual void RemoteControl();

	LLONG GetLoginUserID();
protected:
	// 开始取流
	bool StartRealPlay(REALDATACALLBACK callback, void *userData = NULL/*, HWND hPlayWnd = 0*/);
	// 停止取流
	void StopRealPlay();

public:
	DHIPC(std::string id, Config *config = NULL, int channel = 1);
	virtual ~DHIPC();
	// 取流
	virtual bool StartStreaming();
	// 停止取流
	virtual void StopStreaming();
	// 开始解码
	virtual bool StartDecoding(/*HWND hPlayWnd*/);
	// 停止解码
	virtual void StopDecoding();
	// 抓拍图片
	virtual bool Capture(JPEGPic& capPic);
	// 抓拍图片 设备抓图
	virtual bool CaptureNEW(JPEGPic& capPic);
	// 设置云台角度: 0 - 成功; -1 - 失败
	virtual int Set_DVR_Config(LONG p, LONG t, LONG z);
	// 获取云台角度: 0 - 成功; -1 - 失败
	virtual int Get_DVR_Config(DH_PTZ_LOCATION_INFO* pos);
	// 云台控制操作: 0 - 成功; -1 - 失败(需要启动预览)
	virtual int Set_DVR_PTZControl(int dwPTZCommand, int dwStop);
	// 云台控制操作: 0 - 成功; -1 - 失败(无需启动预览)
	virtual int Set_DVR_PTZControl_Other(int dwPTZCommand, int dwStop);
};
#endif