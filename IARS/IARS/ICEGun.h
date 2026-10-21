#ifndef ICEGUN_H
#define ICEGUN_H

#include "Utils.h"
#include "CameraConfig.h"
#include <ice_ipcsdk/ice_ipcsdk.h>
#include <ice_ipcsdk/ice_comdef.h>


// 智通视讯SDK通用接口
class CICEGun
{
protected:
    HANDLE m_handle;

	ICEConfig *m_devCfg;
public:
    // 登录
    virtual bool LogIn();

    // 登出
    virtual void LogOut();

	int m_lane;
	bool isOnlineState;
    // 返回 IP 地址
    std::string GetDeviceIP();

	std::string GetDeviceLane();
	std::string GetDeviceName();
	// 清理识别结果
	void ClearPlateResult(){
		m_LicPlateResult.nBelieve = 0;
		m_LicPlateResult.sLicPlate = "";
	}

    bool TriggerExt();
	bool CaptureNEW(JPEGPic& rrImage);
	void DealPlateResult(const ICE_CHAR *pcIP, const ICE_CHAR *pcNumber,
		ICE_CHAR *pcPicData, ICE_U32 u32PicLen,
		ICE_CHAR *pcCloseUpPicData, ICE_U32 u32CloseUpPicLen,
		ICE_FLOAT fPlateConfidence);

	std::mutex m_mtxPlateRet;
    LicPlateResult m_LicPlateResult;

	TaskManager *m_pTaskMgr;
public:
	CICEGun(std::string ip, ICEConfig *cfg, TaskManager*pMgr);

    virtual ~CICEGun();
};
#endif