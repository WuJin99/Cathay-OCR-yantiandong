#ifndef DEVIEMANAGER_H
#define DEVIEMANAGER_H

#include "AppConfig.h"
#include "Console.h"
#include "CameraConfig.h"
#include "HKGun.h"
#include "ICEGun.h"
#include "IPCamera.h"
#include "dhnetsdk.h"
#include "CDHCam.h"
class CDeviceManager
{
    /*
    1. 控制全部相机  设备登录、登出等操作。
    2. 控制全部相机  故障检测、处理。
    3. 控制全部相机  
    */
public:
    CDeviceManager();

    ~CDeviceManager();

    void Start(string side, string lane, string cfg, TaskManager *pTaskMgr);

    void Stop();

	void LoadDevCfg(string xml_path);

	// 枪机配置列表
	std::map<std::string, HKGunConfig*> gunCfgList;
	std::map<std::string, CHKGun*> gunDevList;
	std::map<std::string, HKPlateConfig*> plateCfgList;
	std::map<std::string, CHKPlate*> plateDevList;
	//// 枪机设备列表
	std::map<std::string, CDHCam*> dhDevList;
	//// 枪机配置列表
	std::map<std::string, DHCamConfig*> dhCfgList;
	//AxisF44配置列表
	std::map<std::string, AxisF44Config*> AxisCfgList;
	std::map<std::string, IPCamera*> AxisDevList;

	//车牌相机配置列表
	std::map<std::string, ICEConfig*> iceCfgList;
	std::map<std::string, CICEGun*> iceDevList;

private:
    std::thread worker;		// 相机工作状态监控线程
    //std::thread listener;	// 工作线程
    bool exitFlag;
	//大华相机回调函数
	static int  CALLBACK RealLoadPicCallback(LLONG lAnalyzerHandle, DWORD dwAlarmType, void* pAlarmInfo, BYTE *pBuffer, DWORD dwBufSize, LDWORD dwUser, int nSequence, void *userdata);
	static void CALLBACK DevReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);
	static void CALLBACK DevDisConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);
	static void CALLBACK DevSubDisConnect(EM_INTERFACE_TYPE emInterfaceType, BOOL bOnline, LLONG lOperateHandle, LLONG lLoginID, LDWORD dwUser);
};


#endif //DEVIEMANAGER_H