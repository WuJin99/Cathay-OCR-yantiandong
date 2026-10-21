
#include "ICEGun.h"
#include "TaskManager.h"
#include "AppConfig.h"

#pragma comment(lib, "ice_ipcsdk.lib")

/**
*  @brief  抓拍数据回调接口
*  @param  [OUT] pvParam                 回调参数，能区分开不同的使用者即可（open传入的最后一个参数）
*  @param  [OUT] pcIP                    相机ip
*  @param  [OUT] pcNumber                车牌号
*  @param  [OUT] pcColor                 车牌颜色）（"蓝色","黄色","白色","黑色",“绿色”）
*  @param  [OUT] pcPicData               全景数据
*  @param  [OUT] u32PicLen               全景数据长度
*  @param  [OUT] pcCloseUpPicData        车牌数据
*  @param  [OUT] u32CloseUpPicLen        车牌数据长度
*  @param  [OUT] s16Speed                速度
*  @param  [OUT] s16VehicleType          车辆类型
*  @param  [OUT] s16FalsePlate           虚假车牌
*  @param  [OUT] s16Reserved2            预留参数2
*  @param  [OUT] fPlateConfidence        车牌打分值 详解：SDK输出的范围大于IE界面设置的车牌阈值，上限是28，例如：IE设置的是10，范围：10-28
*                                                   作用：sdk输出的车牌打分值是用在双相机方案，sdk收到两个相机的车牌信息，可以通过车牌
*                                                   打分值来选择哪个信息更加可靠作为最终的输出结果
*  @param  [OUT] u32VehicleColor         车身颜色
*  @param  [OUT] u32PlateType            车牌类型
*  @param  [OUT] u32VehicleDir           车辆方向
*  @param  [OUT] u32AlarmType            报警类型
*  @param  [OUT] u32SerialNum            抓拍的序号（从相机第一次抓拍开始计数，相机重启后才清零）
*  @param  [OUT] uCapTime                实时抓拍时间，从1970年1月1日零点开始的秒数
*  @param  [OUT] u32ResultHigh           车牌识别数据结构体(ICE_VDC_PICTRUE_INFO_S)指针高8位
*  @param  [OUT] u32ResultLow            车牌识别数据结构体(ICE_VDC_PICTRUE_INFO_S)指针低8位
*/
void __stdcall SDK_ICE_PLATE(void *pvParam, const ICE_CHAR *pcIP,
    const ICE_CHAR *pcNumber, const ICE_CHAR *pcColor,
    ICE_CHAR *pcPicData, ICE_U32 u32PicLen, ICE_CHAR *pcCloseUpPicData, ICE_U32 u32CloseUpPicLen,
    ICE_S16 s16Speed, ICE_S16 s16VehicleType, ICE_S16 s16FalsePlate, ICE_S16 s16Reserved2,
    ICE_FLOAT fPlateConfidence, ICE_U32 u32VehicleColor, ICE_U32 u32PlateType, ICE_U32 u32VehicleDir,
    ICE_U32 u32AlarmType, ICE_U32 u32SerialNum, ICE_U32 uCapTime, ICE_U32 u32ResultHigh, ICE_U32 u32ResultLow)
{
	CICEGun *pThis = reinterpret_cast<CICEGun*>(pvParam);
    if (pThis)
    {
		pThis->DealPlateResult(pcIP, pcNumber, pcPicData, u32PicLen, pcCloseUpPicData, u32CloseUpPicLen, fPlateConfidence);
    }
}

/**
*  @brief  通过该回调函数获取相机的连接状态
*  @param  [OUT] pvParam             相机连接回调参数, 用于区分不同相机连接事件(ICE_IPCSDK_SetDeviceEventCallBack传入的最后一个参数)
*  @param  [OUT] pcIP                相机ip
*  @param  [OUT] u32EventType        事件类型 0：离线 1：在线 2:IO变化
*  @param  [OUT] u32EventData1       事件数据1 事件类型为0或者1时，代表回调句柄;事件类型为2时，代表IO1的状态;）
*  @param  [OUT] u32EventData2       事件数据2 （事件类型为2时，代表IO2的状态）
*  @param  [OUT] u32EventData3       事件数据3 （事件类型为2时，代表IO3的状态）
*  @param  [OUT] u32EventData4       事件数据4 （事件类型为2时，代表IO4的状态）
*/
void __stdcall SDK_ICE_DEVICEEVENT(void *pvParam, const ICE_CHAR *pcIP, ICE_U32 u32EventType,
	ICE_U32 u32EventData1, ICE_U32 u32EventData2, ICE_U32 u32EventData3, ICE_U32 u32EventData4)
{
	CICEGun *pThis = reinterpret_cast<CICEGun*>(pvParam);
	if (pThis)
	{
		if (0 == u32EventType)
		{
			console->error("车道[{}]车牌相机[{}] 离线", pThis->GetDeviceLane(), pcIP);
			pThis->isOnlineState = false;
			console->warn("相机状态FAULT, IP={}", pcIP);
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, pThis->GetDeviceName(), "离线",
				StateInfo_RUN_STATE_RUN_FAULT, pThis->GetDeviceIP());
		}
		else if (1 == u32EventType)
		{
			console->debug("车道[{}]车牌相机[{}] 在线", pThis->GetDeviceLane(), pcIP);
			pThis->isOnlineState = true;
			console->warn("相机状态NORMAL, IP={}", pcIP);
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, pThis->GetDeviceName(), "在线",
				StateInfo_RUN_STATE_RUN_NORMAL, pThis->GetDeviceIP());
		}
	}
}

void CICEGun::DealPlateResult(const ICE_CHAR *pcIP, const ICE_CHAR *pcNumber,
	ICE_CHAR *pcPicData, ICE_U32 u32PicLen,
	ICE_CHAR *pcCloseUpPicData, ICE_U32 u32CloseUpPicLen,
	ICE_FLOAT fPlateConfidence)
{
	if (!isOnlineState)
	{
		isOnlineState = true;
		console->warn("相机状态NORMAL, IP={}", pcIP);
	}
	// 车号
	std::lock_guard<std::mutex> lck(m_mtxPlateRet);
	m_LicPlateResult.sLicPlate = pcNumber;

	// 置信度,车牌阈值范围：10-28，实际置信度=100/(28-10)*(fPlateConfidence - 10)
	m_LicPlateResult.nBelieve = 5.55 * (fPlateConfidence - 10);
	if (m_LicPlateResult.nBelieve < 0)
		m_LicPlateResult.nBelieve = 0;
	if (m_LicPlateResult.sLicPlate.find("无牌车")!=string::npos)
	{	
		//m_LicPlateResult.sLicPlate = "";
		m_LicPlateResult.nBelieve = 99;
	}

	//识别时间
	m_LicPlateResult.uiRecong = GetTickCount();
	m_LicPlateResult.strRecTime = GetSysTimeString();
	console->debug("车道[{}]车牌相机[{}] 识别车牌号码[{}],置信度[{}]", GetDeviceLane(), pcIP, pcNumber, m_LicPlateResult.nBelieve);
	if (m_pTaskMgr->m_taskState == 1)
	{
		// 车辆照片
		if (u32PicLen > m_LicPlateResult.vehPic.size)
		{
			if (m_LicPlateResult.vehPic.buf)
				delete[]m_LicPlateResult.vehPic.buf;
			m_LicPlateResult.vehPic.buf = new BYTE[u32PicLen];
		}
		if (u32PicLen > 0)
			memcpy(m_LicPlateResult.vehPic.buf, pcPicData, u32PicLen);
		m_LicPlateResult.vehPic.size = u32PicLen;


		std::string dir = g_AppConfig.userCfg.savePath + "\\" + m_pTaskMgr->m_task_dir + "\\";
		MakeDirs(dir);
		std::string strIP = pcIP;
		//保存车辆图片
		std::string name = GetSysTimeString() + "_" + strIP + "_" + std::to_string(m_LicPlateResult.nBelieve);
		std::string path = dir + name + "_vehicle.jpg";
		WriteImage(path, (BYTE*)pcPicData, u32PicLen);
		console->info("保存车牌相机拍摄图片 path:{}", path);

		//将包含车牌结果的图片单独保存，方便按车牌搜索
		path = dir + name + "_" + m_LicPlateResult.sLicPlate + "_vehicle.jpg";
		WriteImage(path, (BYTE*)pcPicData, u32PicLen);

		cv::Mat jpeg(1, u32PicLen, CV_8UC1, (BYTE*)pcPicData);
		shared_ptr<cv::Mat> pImg = make_shared<cv::Mat>();
		cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());
		//保存车牌照片
		if (u32CloseUpPicLen)
		{
			path = dir + name + "_plate.jpg";
			WriteImage(path, (BYTE*)pcCloseUpPicData, u32CloseUpPicLen);
			console->info("保存车牌相机拍摄车牌图片 path:{}", path);
		}

		m_LicPlateResult.sFileName = name + "_vehicle.jpg";
		m_LicPlateResult.lane = m_lane;
		if (m_LicPlateResult.nBelieve > 0)
			m_pTaskMgr->SetPlateResult(m_LicPlateResult, pImg);
	}
}

bool CICEGun::LogIn()
{
	m_LicPlateResult.sCamIp = m_devCfg->ip;
	m_LicPlateResult.sLane = m_pTaskMgr->m_lane_id;
	m_lane = m_devCfg->lane;
	m_LicPlateResult.lane = m_devCfg->lane;

    int nConn = 0;
    do
    {
	    m_handle = ICE_IPCSDK_OpenDevice(m_devCfg->ip.c_str());
		if (m_handle)
		{
			ICE_IPCSDK_SetPlateCallback(m_handle, SDK_ICE_PLATE, this);
			ICE_IPCSDK_SetDeviceEventCallBack(m_handle, SDK_ICE_DEVICEEVENT, this);
			ICE_CameraInfo cam_info;
			if (ICE_IPCSDK_GetCameraInfo(m_handle, &cam_info))
			{
				console->info("车道[{}]车牌相机[{}] 版本时间[{}] 相机app版本[{}] 相机算法版本[{}]", 
					GetDeviceLane(), GetDeviceIP(), cam_info.szAppTime, cam_info.szAppVersion, cam_info.szAlgoVersion);
			}
		}
        nConn++;

        if (m_handle == NULL)
        {
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, m_devCfg->name, "连接失败",
				StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());
            console->error("车牌相机[{}]第[{}]连接失败", m_devCfg->ip.c_str(), nConn);
			//30秒重连
			Sleep(30000); //wxm 2021.3.22
        }
    } while (m_handle == NULL);

    console->info("车牌相机[{}]连接成功", m_devCfg->ip.c_str());

    return 0;
}

void CICEGun::LogOut()
{
	if (m_handle)
	{
		ICE_IPCSDK_Close(m_handle);
		m_handle = NULL;
	}
}

std::string CICEGun::GetDeviceIP()
{
    return m_devCfg->ip;
}

std::string CICEGun::GetDeviceLane()
{
	return m_pTaskMgr->m_lane_id;
}

std::string CICEGun::GetDeviceName()
{
	return m_devCfg->name;
}

bool CICEGun::TriggerExt()
{
	if (m_handle)
		return ICE_IPCSDK_TriggerExt(m_handle);//软触发扩展接口，不返回识别数据
	return false;
}

bool CICEGun::CaptureNEW(JPEGPic& rrImage)
{
	if (!m_handle)
		return false;
	unsigned int nPicLen = 0;
	if (rrImage.buf == NULL)
		rrImage.buf = new BYTE[MAX_JPEG_BUFSIZE];
	ICE_IPCSDK_Capture(m_handle, (char*)rrImage.buf, MAX_JPEG_BUFSIZE, &nPicLen);
	return nPicLen > 0;
}

CICEGun::CICEGun(std::string ip, ICEConfig *cfg, TaskManager*pMgr)
    : m_handle(NULL)
    , m_devCfg(cfg)
	, m_pTaskMgr(pMgr)
{
	ClearPlateResult();
	
	std::thread worker = std::thread
	{
		[this]()
		{
			console->info("车牌相机[{}/{}]状态监控线程启动", GetDeviceLane(), GetDeviceIP());
			int nWaitSec = g_AppConfig.userCfg.stateInfoInterval > 0 ? g_AppConfig.userCfg.stateInfoInterval : 60;
			while (true)
			{
				if (!m_handle)
				{
					LogIn();
				}
				//检测相机是否在线
				std::this_thread::sleep_for(std::chrono::seconds(nWaitSec));
				if (0 == ICE_IPCSDK_GetStatus(m_handle))
				{
					std::string msg = "相机[";
					msg += GetDeviceIP();
					msg += "]断线重连";

					//离线
					isOnlineState = false;
					console->warn("相机状态FAULT, IP={}", GetDeviceIP());
					console->error("车牌相机[{}/{}]离线，重新连接", GetDeviceLane(), GetDeviceIP());
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, m_devCfg->name, "断线重连",
						StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());
					ICE_IPCSDK_Close(m_handle);
					m_handle = NULL;
					LogIn();
				}
				else
				{
					isOnlineState = true;
					console->warn("相机状态NORMAL, IP={}", GetDeviceIP());
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, m_devCfg->name, "运行正常",
						StateInfo_RUN_STATE_RUN_NORMAL, GetDeviceIP());
				}
			};
		}
	};
	worker.detach();
}

CICEGun::~CICEGun()
{
	LogOut();
}
