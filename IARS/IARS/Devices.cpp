#include "Devices.h"



CDevice::CDevice(std::string id, Config *config /*= NULL*/)
	: camID(id)
	, m_devCfg(config)
	, stateType(StateInfo_RUN_STATE_RUN_FAULT)
	, oldStateType(StateInfo_RUN_STATE_RUN_FAULT)
	, m_getCnResult(false)
	, m_getPmnResult(false)
	//, eventInfoPath("EventInfo/")
{
	//MakeDirs(eventInfoPath);
}

bool CDevice::Capture(JPEGPic& capPic)
{
    capPic.size = 0;
    return false;
}

bool CDevice::CaptureNEW(JPEGPic& capPic)
{
    capPic.size = 0;
    return false;
}
void CDevice::SetStateType(StateInfo::RUN_STATE type)
{
	stateTypeMutex.lock();
	stateType = type;
	stateTypeMutex.unlock();
}
bool CDevice::OnCmdMsg(int nID)
{
    // 登录/登出菜单处理
    if (nID == DEVICE_LOGIN)
    {
        // 已登录
        if (HasLogIn())
        {
            LogOut();
        }
        else
        {
            return LogIn();
        }
        return true;
    }
    // 开始/停止取流
    else if (nID == DEVICE_STREAMING_START)
    {

        // 未预览
        if (IsStreaming() == false)
        {
            // 取流
            StartStreaming();
        }
        else
        {
            // 停止取流
            StopStreaming();
        }

        return true;
    }
    // 开始/停止显示视频
    else if (nID == DEVICE_VIDEO_SHOW)
    {

        if (IsDecoding() == false)
        {

            StartDecoding();
        }
        else
        {
            StopDecoding();
        }
        return true;
    }
    // 抓拍
    else if (nID == DEVICE_CAPTURE)
    {
        JPEGPic capPic;
        if (IsDecoding())
        {
            Capture(capPic);
        }
        else
        {
            CaptureNEW(capPic);
        }
        if (capPic.buf && capPic.size > 0)
        {
            ;
        }
        return true;
    }
    // 云台
    else if (nID == DEVICE_PTZ)
    {
        return true;
    }
    return false;
}

void CDevice::UpdateStatus(Status s)
{
    m_status = s;
}

bool CHKIPC::LogIn()
{
    const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
    if (m_loginUserID < 0)
    {
		console->error("{} 重新登录:",ipcCfg->ip);
        NET_DVR_USER_LOGIN_INFO loginInfo;
        memset(&loginInfo, 0, sizeof(loginInfo));

        loginInfo.bUseAsynLogin = FALSE;
        // 用户名
        _snprintf_s(loginInfo.sUserName, sizeof(loginInfo.sUserName), "%s", ipcCfg->userName.c_str());
        // 密码
        _snprintf_s(loginInfo.sPassword, sizeof(loginInfo.sPassword), "%s", ipcCfg->passwd.c_str());
        // IP
        _snprintf_s(loginInfo.sDeviceAddress, sizeof(loginInfo.sDeviceAddress), "%s", ipcCfg->ip.c_str());
        // 端口
        loginInfo.wPort = ipcCfg->nPort;

        // SDK登录接口
        NET_DVR_DEVICEINFO_V40 deviceInfo;
        memset(&deviceInfo, 0, sizeof(deviceInfo));
        console->info("Gun [{:>10}] {}:{}@{}:{}", GetDeviceIP(), loginInfo.sUserName, loginInfo.sPassword, loginInfo.sDeviceAddress, loginInfo.wPort);
        m_loginUserID = NET_DVR_Login_V40(&loginInfo, &deviceInfo);
    }
    UpdateStatus(LOGGEDIN);
    if (m_loginUserID < 0)
    {
        UpdateStatus(LOGGEDOUT);
        // 提示登录失败的原因
        LONG en = NET_DVR_GetLastError();
        if (en != NET_DVR_NOERROR)
        {
            console->error("{} 登录失败: {}", ipcCfg->ip, NET_DVR_GetErrorMsg(&en));
        }

		stateType = StateInfo_RUN_STATE_RUN_FAULT;
		oldStateType = stateType;
    }
	else
	{  
		
		stateType = StateInfo_RUN_STATE_RUN_NORMAL;
		oldStateType = stateType;
	}
    return HasLogIn();
}

void CHKIPC::LogOut()
{
    if (HasLogIn())
    {
        // 停止显示视频
        StopDecoding();
        // 停止取流
        StopStreaming();
        NET_DVR_Logout(m_loginUserID);
        UpdateStatus(LOGGEDOUT);
        m_loginUserID = -1;
    }
}

std::string CHKIPC::GetDeviceIP()
{
    const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
    if (ipcCfg)
    {
        return ipcCfg->ip;
    }
    return "0.0.0.0";
}
bool CHKIPC::GetCnFirst()
{
	const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
	if (ipcCfg)
	{
		return ipcCfg->cnFirst;
	}
	return false;
}
std::string CHKIPC::GetInstallNmae()
{
    const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
    if (ipcCfg)
    {
        return ipcCfg->camName;
    }
    return "未知";
}

LONG CHKIPC::GetLoginUserID()
{
	return m_loginUserID;
}
void CHKIPC::RemoteControl()
{
    if (!NET_DVR_RemoteControl(m_loginUserID, NET_DVR_CHECK_USER_STATUS, NULL, 0))
    {
		SetStateType(StateInfo_RUN_STATE_RUN_FAULT);
		LogOut();
		console->error("{} 相机当前检测离线: 状态{} 上次状态{}", GetDeviceIP(), stateType, oldStateType);
		console->warn("相机状态FAULT, IP={}", GetDeviceIP());

		if (oldStateType != stateType)
		{
			LONG en = NET_DVR_GetLastError();
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae() , NET_DVR_GetErrorMsg(&en),
				StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());
			oldStateType = stateType;

			console->warn("{} 相机在线状态监测 失败:{}", GetDeviceIP(), NET_DVR_GetErrorMsg(&en));
		}
		console->debug("{} 相机尝试重连", GetDeviceIP());
		if (HasLogIn())
		{
			console->debug("{} HasLogin True", GetDeviceIP());
		}
		else
		{
			console->debug("{} HasLogin False", GetDeviceIP());
		}
		if (!LogIn())
		{
			console->info("{} 相机重连失败", GetDeviceIP());
		}
		else
		{
			console->info("{} 相机重连成功", GetDeviceIP());
		}
    }
	else
	{
		SetStateType(StateInfo_RUN_STATE_RUN_NORMAL);
		console->info("{} 相机当前检测在线: 状态{} 上次状态{}", GetDeviceIP(), stateType, oldStateType);
		console->warn("相机状态NORMAL, IP={}", GetDeviceIP());

		if (oldStateType != stateType)
		{
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), "cam check",
				StateInfo_RUN_STATE_RUN_NORMAL, GetDeviceIP());
			oldStateType = stateType;

			console->warn("{} 相机在线状态监测 成功", GetDeviceIP());
		}
	}
}

bool CHKIPC::StartRealPlay(REALDATACALLBACK callback, void *userData /*= NULL*//*, HWND hPlayWnd = 0*/)
{
    // 尝试登录相机
    if (HasLogIn() == false && LogIn() == false)
    {
        console->warn("{} 启动流预览失败: 无法登录相机", GetDeviceIP());
        return false;
    }

    if (m_lRealPlayHandle < 0)
    {
        NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
        struPlayInfo.hPlayWnd = 0/*hPlayWnd*/;    // 需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
        struPlayInfo.lChannel = m_lChannel;       // 预览通道号
		//预览子码流，避免相机个数太多以及分辨率高导致CPU占用过高。yeming-20220727
        struPlayInfo.dwStreamType = /*0*/1;            // 0-主码流，1-子码流，2-码流3，3-码流4，以此类推
        struPlayInfo.dwLinkMode = 0;              // 0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP
        m_lRealPlayHandle = NET_DVR_RealPlay_V40(m_loginUserID, &struPlayInfo, callback, userData);
    }

    if (m_lRealPlayHandle >= 0)
    {
        UpdateStatus(STREAMMING);
        if (m_bDecoding)
        {
            UpdateStatus(DECODING);
        }
        return true;
    }
    else
    {
        LONG en = NET_DVR_GetLastError();
        console->warn("{} 启动流预览失败: {}", GetDeviceIP(), NET_DVR_GetErrorMsg(&en));
        return false;
    }
}

void CHKIPC::StopRealPlay()
{
    // 停止取流
    if (m_lRealPlayHandle >= 0)
    {
        NET_DVR_StopRealPlay(m_lRealPlayHandle);
        m_lRealPlayHandle = -1;
    }
}

CHKIPC::CHKIPC(std::string id, Config *config /*= NULL*/, int channel /*= 1*/)
    : CDevice(id, config)
    , jpegParam({ 0xff, 0 })
    , m_loginUserID(-1)
    , m_lChannel(channel)
    , m_lRealPlayHandle(-1)
    , m_lPort(-1)
    , m_bDecoding(false)
   /* , m_hPlayWnd(nullptr)*/
{

}

CHKIPC::~CHKIPC()
{
    if (HasLogIn())
    {
        LogOut();
    }
}

bool CHKIPC::StartStreaming()
{
    // 已在取流
    if (IsStreaming())
    {
        if (m_bDecoding)
        {
            UpdateStatus(DECODING);
        }
        return true;
    }

    // 调用海康的播放库来解码流数据
    auto callback = [](
        LONG lRealHandle,
        DWORD dwDataType,   // 数据流类型,用于解码
        BYTE *pBuffer,      // 解码时的头数据
        DWORD dwBufSize,    // 头长度
        void* dwUser        // 用户传递的参数
        )
    {
        auto ipc = static_cast<CHKIPC *>(dwUser);
        switch (dwDataType)
        {
        case NET_DVR_SYSHEAD:       //系统头
            if (!PlayM4_GetPort(&ipc->m_lPort))  //获取播放库未使用的通道号
            {
                console->warn("{} 分配播放通道失败: {}", ipc->GetDeviceIP(), PlayM4_GetLastError(ipc->m_lPort));
                break;
            }
            if (dwBufSize > 0)
            {
                if (!PlayM4_SetStreamOpenMode(ipc->m_lPort, STREAME_REALTIME))  //设置实时流播放模式
                {
                    console->warn("{} 设置实时流播放模式失败: {}", ipc->GetDeviceIP(), PlayM4_GetLastError(ipc->m_lPort));
                    break;
                }
                if (!PlayM4_OpenStream(ipc->m_lPort, pBuffer, dwBufSize, 1024 * 1024)) //打开流接口
                {
                    console->warn("{} 打开流失败: {}", ipc->GetDeviceIP(), PlayM4_GetLastError(ipc->m_lPort));
                    break;
                }
                if (!PlayM4_Play(ipc->m_lPort, ipc->m_hPlayWnd)) //播放开始
                {
                    console->warn("{} 播放视频流失败: {}", ipc->GetDeviceIP(), PlayM4_GetLastError(ipc->m_lPort));
                    break;
                }
            }
            break;
        case NET_DVR_STREAMDATA:    //码流数据
            // 当需要解码时才将码流数据传入播放库
            if (ipc->m_bDecoding && dwBufSize > 0 && ipc->m_lPort != -1)
            {
                if (!PlayM4_InputData(ipc->m_lPort, pBuffer, dwBufSize))
                {
                    break;
                }
            }
            break;
        default:                    //其他数据
            if (ipc->m_bDecoding && dwBufSize > 0 && ipc->m_lPort != -1)
            {
                if (!PlayM4_InputData(ipc->m_lPort, pBuffer, dwBufSize))
                {
                    break;
                }
            }
            break;
        }
    };
    return StartRealPlay(static_cast<REALDATACALLBACK>(callback), this);
}

void CHKIPC::StopStreaming()
{
    // 停止解码
    StopDecoding();
    // 停止播放,取流
    StopRealPlay();
    //释放播放库资源
    if (m_lPort != -1)
    {
        PlayM4_Stop(m_lPort);
        PlayM4_CloseStream(m_lPort);
        PlayM4_FreePort(m_lPort);
        m_lPort = -1;
    }
    if (m_status > LOGGEDIN)
    {
        UpdateStatus(LOGGEDIN);
    }
}

bool CHKIPC::StartDecoding(/*HWND hPlayWnd*/)
{
    // 由于设置了播放窗体重新取流才生效
    //if (IsStreaming()/* && hPlayWnd != m_hPlayWnd*/)
    //{
    //    StopStreaming();
    //}
    /*m_hPlayWnd = hPlayWnd;*/
    m_bDecoding = true;
    return StartStreaming();
}

void CHKIPC::StopDecoding()
{
    m_bDecoding = false;
    
    if (m_status > STREAMMING)
    {
        UpdateStatus(STREAMMING);
    }
}

bool CHKIPC::Capture(JPEGPic& capPic)
{
    // 创建buf
    if (capPic.buf == NULL)
    {
        capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
    }

    if (!IsDecoding())
    {
        console->info("[{:>10}] [抓拍] 检测到未解码, 开启解码", GetDeviceIP());
        StartDecoding(/*m_hPlayWnd*/);
    }

    capPic.size = 0;
    // 流抓拍
    BOOL ret = PlayM4_GetJPEG(m_lPort, capPic.buf, MAX_JPEG_BUFSIZE, (DWORD *)&capPic.size);
    if (!ret)
    {
        capPic.size = 0;
		delete capPic.buf;
		capPic.buf = nullptr;
        LONG en = NET_DVR_GetLastError();
        console->error("[{:>10}] 抓拍失败：{}", GetDeviceIP(), NET_DVR_GetErrorMsg(&en));
        return false;
    }
    return true;
}

bool CHKIPC::CaptureNEW(JPEGPic& capPic)
{
    if (HasLogIn() == false && LogIn() == false)
    {
        console->warn("{}失败: 无法登录相机", GetDeviceIP());
        return false;
    }
    // 创建buf
    if (capPic.buf == NULL)
    {
        capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
    }
    BOOL ret = NET_DVR_CaptureJPEGPicture_NEW(m_loginUserID, m_lChannel, &jpegParam,
        (char *)capPic.buf, MAX_JPEG_BUFSIZE, (LPDWORD)&capPic.size);
    if (!ret)
    {
        capPic.size = 0;
		delete capPic.buf;
		capPic.buf = nullptr;
        LONG en = NET_DVR_GetLastError();
        console->error("[{:>10}] 抓拍失败：{}", GetDeviceIP(), NET_DVR_GetErrorMsg(&en));
        return false;
    }
	// test qb
	//cv::Mat jpeg(1, capPic.size, CV_8UC1, capPic.buf); 
	//cv::Mat rgb;
	//cv::imdecode(jpeg, cv::IMREAD_COLOR, &rgb);
	//imwrite("./qiutest.jpg", rgb);
    return true;
}

int CHKIPC::Set_DVR_Config(NET_DVR_PTZPOS& pos)
{
    if (NET_DVR_SetDVRConfig(m_loginUserID, NET_DVR_SET_PTZPOS, m_lChannel, &pos, sizeof(pos)) == TRUE)
    {
        return 0;
    }
    else
    {
        LONG en = NET_DVR_GetLastError();
        console->warn("[{:>10}] 跳转预置点失败:[{}]{}", GetDeviceIP(), en, NET_DVR_GetErrorMsg(&en));
        return -1;
    }
}

int CHKIPC::Get_DVR_Config(NET_DVR_PTZPOS& pos)
{
    DWORD dwReturn;
    if (NET_DVR_GetDVRConfig(m_loginUserID, NET_DVR_GET_PTZPOS, m_lChannel, &pos, sizeof(pos), &dwReturn) == FALSE)
    {
        LONG ErrorNo = NET_DVR_GetLastError();
        if (ErrorNo != NET_DVR_NOINIT)
        {
            console->warn("[{:>10}] 获取PTZ失败:[{}]{}", GetDeviceIP(), ErrorNo, NET_DVR_GetErrorMsg(&ErrorNo));
            return -1;
        }
    }
    return 0;
}

int CHKIPC::Set_DVR_PTZControl(int dwPTZCommand, int dwStop)
{
    if (NET_DVR_PTZControl(m_lRealPlayHandle,dwPTZCommand,dwStop) == FALSE)
    {
        LONG ErrorNo = NET_DVR_GetLastError();
        if (ErrorNo != NET_DVR_NOINIT)
        {
            console->warn("[{:>10}] 设置PTZControl失败:[{}]{}", GetDeviceIP(), ErrorNo, NET_DVR_GetErrorMsg(&ErrorNo));
            return -1;
        }
    }
    return 0;
}

int CHKIPC::Set_DVR_PTZControl_Other(int dwPTZCommand, int dwStop)
{
    if (NET_DVR_PTZControl_Other(m_loginUserID, 101, dwPTZCommand, dwStop) == FALSE)
    {
        LONG ErrorNo = NET_DVR_GetLastError();
        if (ErrorNo != NET_DVR_NOINIT)
        {
            console->warn("[{:>10}] 设置PTZControl_Other失败:[{}]{}", GetDeviceIP(), ErrorNo, NET_DVR_GetErrorMsg(&ErrorNo));
            return -1;
        }
    }
    return 0;
}

//DH 大华


bool DHIPC::LogIn()
{
	const  DHCamConfig *ipcCfg = static_cast<const DHCamConfig *>(m_devCfg);
	if (m_loginUserID == 0)
	{





		NET_IN_LOGIN_WITH_HIGHLEVEL_SECURITY stInparam;
		memset(&stInparam, 0, sizeof(stInparam));
		stInparam.dwSize = sizeof(stInparam);
		sprintf_s(stInparam.szIP, "%s", ipcCfg->ip.c_str());
		sprintf_s(stInparam.szPassword, "%s", ipcCfg->passwd.c_str());
		sprintf_s(stInparam.szUserName, "%s", ipcCfg->userName.c_str());
		stInparam.nPort = ipcCfg->nPort;
		stInparam.emSpecCap = EM_LOGIN_SPEC_CAP_TCP;

		NET_OUT_LOGIN_WITH_HIGHLEVEL_SECURITY stOutparam;
		memset(&stOutparam, 0, sizeof(stOutparam));
		stOutparam.dwSize = sizeof(stOutparam);
		m_loginUserID = CLIENT_LoginWithHighLevelSecurity(&stInparam, &stOutparam);








		//LPNET_DEVICEINFO_Ex lpDeviceInfo = nullptr;
		//int *error = 0;
		//NET_OUT_LOGIN_WITH_HIGHLEVEL_SECURITY outParam;
		//NET_IN_LOGIN_WITH_HIGHLEVEL_SECURITY stInparam;
		//memset(&stInparam, 0, sizeof(stInparam));
		//stInparam.dwSize = sizeof(stInparam);
		//strncpy(stInparam.szIP, ipcCfg->ip.c_str(), sizeof(stInparam.szIP) - 1);
		//strncpy(stInparam.szPassword, ipcCfg->passwd.c_str(), sizeof(stInparam.szPassword) - 1);
		//strncpy(stInparam.szUserName, ipcCfg->userName.c_str(), sizeof(stInparam.szUserName) - 1);
		//stInparam.nPort = ipcCfg->nPort;
		//stInparam.emSpecCap = EM_LOGIN_SPEC_CAP_TCP;
		////m_loginUserID = CLIENT_LoginEx2(ipcCfg->ip.c_str(), ipcCfg->nPort, ipcCfg->userName.c_str(), ipcCfg->passwd.c_str(), EM_LOGIN_SPAC_CAP_TYPE(0), NULL, lpDeviceInfo, error);
		//m_loginUserID = CLIENT_LoginWithHighLevelSecurity(&stInparam, &outParam);
		if (m_loginUserID == 0)
		{
			console->info("DH login Error :[{}]  ip[{}] port[{}]", stOutparam.nError, ipcCfg->ip, ipcCfg->nPort);
		}

		console->info("[{:>10}] {}:{}@{}:{}", GetDeviceIP(), ipcCfg->userName, ipcCfg->passwd, ipcCfg->ip, ipcCfg->nPort);
	}

	UpdateStatus(LOGGEDIN);
	if (m_loginUserID <= 0)
	{
		UpdateStatus(LOGGEDOUT);
		// 提示登录失败的原因
		DWORD en = CLIENT_GetLastError();
		console->warn("{} 相机在线状态监测 失败:{}", GetDeviceIP(), en - 2147483648);

		stateType = StateInfo_RUN_STATE_RUN_FAULT;
		oldStateType = stateType;
	}
	else
	{
		stateType = StateInfo_RUN_STATE_RUN_NORMAL;
		oldStateType = stateType;
		//设置抓图回调
		// 调用海康的播放库来解码流数据
		auto callback = [](
			LLONG  lLoginID,
			BYTE   *pBuf,
			UINT   RevLen,
			UINT   EncodeType,
			DWORD  CmdSerial,
			LDWORD dwUser
			)
		{

		};

		CLIENT_SetSnapRevCallBack(callback, NULL);
	}
	return HasLogIn();
}

void DHIPC::LogOut()
{
	if (HasLogIn())
	{
		// 停止显示视频
		StopDecoding();
		// 停止取流
		StopStreaming();
		CLIENT_Logout(m_loginUserID);
		UpdateStatus(LOGGEDOUT);
		m_loginUserID = -1;
	}
}

std::string DHIPC::GetDeviceIP()
{
	const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
	if (ipcCfg)
	{
		return ipcCfg->ip;
	}
	return "0.0.0.0";
}




std::string DHIPC::GetInstallNmae()
{
	const IPCConfig *ipcCfg = static_cast<const IPCConfig *>(m_devCfg);
	if (ipcCfg)
	{
		return ipcCfg->camName;
	}
	return "未知";
}

LLONG DHIPC::GetLoginUserID()
{
	return m_loginUserID;
}
void DHIPC::RemoteControl()
{
	int ll = sizeof(DWORD);
	int getl;
	DWORD p;
	if (!CLIENT_QueryDevState(m_loginUserID, DH_DEVSTATE_ONLINE, (char *)&p, ll, &getl))
	{

		SetStateType(StateInfo_RUN_STATE_RUN_FAULT);
		if (oldStateType != stateType)
		{
			DWORD en = CLIENT_GetLastError();
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), to_string(en),
				StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());
			console->warn("{} 相机在线状态监测 失败:{}", GetDeviceIP(), en - 2147483648);
			oldStateType = stateType;
		}
	}
	else
	{
		if (p == 0)
		{

			SetStateType(StateInfo_RUN_STATE_RUN_FAULT);
			if (oldStateType != stateType)
			{
				DWORD en = CLIENT_GetLastError();
				PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), to_string(en),
					StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());
				console->warn("{} 相机在线状态监测 失败:{}", GetDeviceIP(), en - 2147483648);
				oldStateType = stateType;
			}
		}
		else
		{

			SetStateType(StateInfo_RUN_STATE_RUN_NORMAL);
			if (oldStateType != stateType)
			{
				PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), "cam check",
					stateType, GetDeviceIP());
				console->warn("{} 相机在线状态监测 成功", GetDeviceIP());
				oldStateType = stateType;
			}
		}
	}
}

bool DHIPC::StartRealPlay(REALDATACALLBACK callback, void *userData /*= NULL*//*, HWND hPlayWnd = 0*/)
{
	// 尝试登录相机
	if (HasLogIn() == false && LogIn() == false)
	{
		console->warn("{} 启动流预览失败: 无法登录相机", GetDeviceIP());
		return false;
	}

	if (m_lRealPlayHandle == 0)
	{
		m_lRealPlayHandle = CLIENT_RealPlayEx(m_loginUserID, m_lChannel, NULL, DH_RealPlayType::DH_RType_Realplay_3);
	}

	if (m_lRealPlayHandle != 0)
	{
		UpdateStatus(STREAMMING);
		if (m_bDecoding)
		{
			UpdateStatus(DECODING);
		}
		return true;
	}
	else
	{
		DWORD en = CLIENT_GetLastError();
		console->warn("{} 启动流预览失败: {}", GetDeviceIP(), en);
		return false;
	}
}

void DHIPC::StopRealPlay()
{
	// 停止取流
	if (m_lRealPlayHandle >= 0)
	{
		CLIENT_StopRealPlayEx(m_lRealPlayHandle);
		m_lRealPlayHandle = -1;
	}

	BOOL isReleasePlatehanle = CLIENT_StopLoadPic(m_plateHandle);
	console->info("释放车牌检测handle:{}", isReleasePlatehanle);
}

DHIPC::DHIPC(std::string id, Config *config /*= NULL*/, int channel /*= 1*/)
	: CDevice(id, config)
	, m_loginUserID(0)
	, m_lChannel(channel)
	, m_lRealPlayHandle(0)
	, m_lPort(-1)
	, m_bDecoding(false)
	/* , m_hPlayWnd(nullptr)*/
{

}

DHIPC::~DHIPC()
{
	if (HasLogIn())
	{
		LogOut();
	}
}

bool DHIPC::StartStreaming()
{
	// 已在取流
	if (IsStreaming())
	{
		if (m_bDecoding)
		{
			UpdateStatus(DECODING);
		}
		return true;
	}
	return true;
}

void DHIPC::StopStreaming()
{
	// 停止解码
	StopDecoding();
	// 停止播放,取流
	StopRealPlay();
	//释放播放库资源
	/*if (m_lPort != -1)
	{
		PlayM4_Stop(m_lPort);
		PlayM4_CloseStream(m_lPort);
		PlayM4_FreePort(m_lPort);
		m_lPort = -1;
	}*/
	if (m_status > LOGGEDIN)
	{
		UpdateStatus(LOGGEDIN);
	}
}

bool DHIPC::StartDecoding(/*HWND hPlayWnd*/)
{
	// 由于设置了播放窗体重新取流才生效
	//if (IsStreaming()/* && hPlayWnd != m_hPlayWnd*/)
	//{
	//    StopStreaming();
	//}
	/*m_hPlayWnd = hPlayWnd;*/
	m_bDecoding = true;
	return StartStreaming();
}

void DHIPC::StopDecoding()
{
	m_bDecoding = false;

	if (m_status > STREAMMING)
	{
		UpdateStatus(STREAMMING);
	}
}

bool DHIPC::Capture(JPEGPic& capPic)
{
	//大华相机取图片数据功能暂时不实现，该项目未使用大华球机。yeming-20250612
	return false;
	//// 创建buf
	//if (capPic.buf == NULL)
	//{
	//	capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
	//}

	//if (!IsDecoding())
	//{
	//	console->info("[{:>10}] [抓拍] 检测到未解码, 开启解码", GetDeviceIP());
	//	StartDecoding(/*m_hPlayWnd*/);
	//}

	//capPic.size = 0;
	//SNAP_PARAMS param;
	//param.Channel = m_lChannel;
	//param.Quality = 6;
	//param.ImageSize = 2;
	//param.mode = 0;

	//// 流抓拍
	//BOOL ret = CLIENT_SnapPictureEx(m_loginUserID, &param, NULL);
	//if (!ret)
	//{
	//	capPic.size = 0;
	//	delete capPic.buf;
	//	capPic.buf = nullptr;
	//	/*
	//	DWORD en = CLIENT_GetLastError();
	//	AddTaskErrorLog(console->error("[{:>10}] 抓拍失败：{}", GetDeviceIP(), NET_DVR_GetErrorMsg(&en)));*/
	//	return false;
	//}
	//int count = 0;
	//
	//return true;
}

bool DHIPC::CaptureNEW(JPEGPic& capPic)
{
	return false;
	//// 创建buf
	//if (capPic.buf == NULL)
	//{
	//	capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
	//}
	//capPic.size = 0;
	//SNAP_PARAMS param;
	//param.Channel = m_lChannel;
	//param.Channel = 0;
	//param.Quality = 6;
	//param.ImageSize = 2;
	//param.mode = 0;

	//// 流抓拍
	//BOOL ret = CLIENT_SnapPictureEx(m_loginUserID, &param, NULL);
	//if (!ret)
	//{
	//	capPic.size = 0;
	//	delete capPic.buf;
	//	capPic.buf = nullptr;
	//	DWORD en = CLIENT_GetLastError();//2,147,483,648
	//	console->error("[{:>10}] 抓拍失败：{}", GetDeviceIP(), en - 2147483648);
	//	return false;
	//}
	//return true;
}

int DHIPC::Set_DVR_Config(LONG p, LONG t, LONG z)
{
	if (CLIENT_DHPTZControlEx2(m_loginUserID, 0, DH_EXTPTZ_EXACTGOTO, p, t, z, 0))
	{
		return 0;
	}
	else
	{
		DWORD en = CLIENT_GetLastError();//2,147,483,648
		console->error("[{:>10}] 跳转预置点失败：{}", GetDeviceIP(), en - 2147483648);
		return -1;
	}
}

int DHIPC::Get_DVR_Config(DH_PTZ_LOCATION_INFO* pos)
{
	char pinfo[] = { 0 };
	int nBufLen = sizeof(DH_PTZ_LOCATION_INFO);
	int pRetLen;
	if (!CLIENT_QueryDevState(m_loginUserID, DH_DEVSTATE_PTZ_LOCATION, (char *)pos, nBufLen, &pRetLen))
	{

		DWORD en = CLIENT_GetLastError();//2,147,483,648
		console->error("[{:>10}] 获取PTZ失败：{}", GetDeviceIP(), en - 2147483648);
		return -1;

	}

	//pos = (DH_PTZ_LOCATION_INFO *)pinfo;

	return 0;
}

int DHIPC::Set_DVR_PTZControl(int dwPTZCommand, int dwStop)
{
	//if (NET_DVR_PTZControl(m_lRealPlayHandle, dwPTZCommand, dwStop) == FALSE)
	//{
	//	LONG ErrorNo = NET_DVR_GetLastError();
	//	if (ErrorNo != NET_DVR_NOINIT)
	//	{
	//		console->warn("[{:>10}] 设置PTZControl失败:[{}]{}", GetDeviceIP(), ErrorNo, NET_DVR_GetErrorMsg(&ErrorNo));
	//		return -1;
	//	}
	//}
	return 0;
}

int DHIPC::Set_DVR_PTZControl_Other(int dwPTZCommand, int dwStop)
{
	//if (NET_DVR_PTZControl_Other(m_loginUserID, 101, dwPTZCommand, dwStop) == FALSE)
	//{
	//	LONG ErrorNo = NET_DVR_GetLastError();
	//	if (ErrorNo != NET_DVR_NOINIT)
	//	{
	//		console->warn("[{:>10}] 设置PTZControl_Other失败:[{}]{}", GetDeviceIP(), ErrorNo, NET_DVR_GetErrorMsg(&ErrorNo));
	//		return -1;
	//	}
	//}
	return 0;
}
