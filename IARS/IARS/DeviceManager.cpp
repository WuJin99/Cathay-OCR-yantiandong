#include "DeviceManager.h"
#include "TaskManager.h"
#include "TimeMeter.hpp"


void CDeviceManager::DevDisConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
	console->info("DevDisConnect LoginID={} ip={} port={}", lLoginID, pchDVRIP, nDVRPort);
	CDeviceManager *pThis = (CDeviceManager*)dwUser;
	auto iter = pThis->dhDevList.find(pchDVRIP);
	if (iter != pThis->dhDevList.end())
	{
		iter->second->stateType = StateInfo_RUN_STATE_RUN_FAULT;
		console->warn("相机状态FAULT, IP={}", pchDVRIP);
	}
	else
	{
		console->error("相机状态FAULT, IP={} 未找到", pchDVRIP);
	}
	return;
}

void CDeviceManager::DevReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
	console->info("DevReConnect LoginID={} ip={} port={}", lLoginID, pchDVRIP, nDVRPort);
	CDeviceManager *pThis = (CDeviceManager*)dwUser;
	auto iter = pThis->dhDevList.find(pchDVRIP);
	if (iter != pThis->dhDevList.end())
	{
		iter->second->stateType = StateInfo_RUN_STATE_RUN_NORMAL;
		console->warn("相机状态NORMAL, IP={}", pchDVRIP);
	}
	else
	{
		console->error("相机状态NORMAL, IP={} 未找到", pchDVRIP);
	}
	return;
}

void CDeviceManager::DevSubDisConnect(EM_INTERFACE_TYPE emInterfaceType, BOOL bOnline, LLONG lOperateHandle, LLONG lLoginID, LDWORD dwUser)
{
	CDeviceManager *pThis = (CDeviceManager*)dwUser;
	console->info("DevSubDisConnect online={} OperateHandle={} LoginID={}", bOnline, lOperateHandle, lLoginID);
	return;
}

CDeviceManager::CDeviceManager()
	: exitFlag(false)
{
	// 海康SDK初始化
	if (!NET_DVR_Init())
	{
		DWORD dwErrCode = NET_DVR_GetLastError();
		console->error("海康SDK初始化失败:{} {}", dwErrCode, NET_DVR_GetErrorMsg());
	}

	CLIENT_Init(DevDisConnect, (LDWORD)this);
	LOG_SET_PRINT_INFO  stLogPrintInfo = { sizeof(stLogPrintInfo) };
	CLIENT_LogOpen(&stLogPrintInfo);
	CLIENT_SetAutoReconnect(DevReConnect, (LDWORD)this);
	CLIENT_SetSubconnCallBack(DevSubDisConnect, (LDWORD)this);
	CLIENT_SetConnectTime(3000, 3);

	DWORD dwVersion = NET_DVR_GetSDKBuildVersion();
	console->info("海康SDK版本:{}.{}.{}.{}",
		(0xff000000 & dwVersion) >> 24,
		(0x00ff0000 & dwVersion) >> 16,
		(0x0000ff00 & dwVersion) >> 8,
		(0x000000ff & dwVersion));
	console->info("ICE SDK初始化");
	ICE_IPCSDK_Init();
	if (g_AppConfig.userCfg.sdk_log)
	{
		std::string logPath = GetAppPath() + "\\ConsoleLog\\";
		if (!NET_DVR_SetLogToFile(3, (char*)logPath.c_str(), TRUE))
		{
			console->error("海康SDK启用日志失败:{} {}", NET_DVR_GetLastError(), NET_DVR_GetErrorMsg());
		}
		ICE_IPCSDK_LogConfig(1, (char*)logPath.c_str());
	}
}

CDeviceManager::~CDeviceManager()
{
}

void CDeviceManager::Start(string side, string lane, string cfg, TaskManager *pTaskMgr)
{
	console->info("{} 设备管理<模块启动>", side);

	// 设备列表初始化
	// 加载CameraConfig配置
	LoadDevCfg(cfg);

	// 加载海康相机
	for (auto&t : gunCfgList)
	{
		auto gun = new CHKGun(t.second->ip, t.second, pTaskMgr);
		gunDevList[gun->GetDeviceIP()] = gun;
		console->info("GUN加载成功 名称:{}, IP:{}", t.second->camName, t.second->ip);
	}
	for (auto&t : plateCfgList)
	{
		auto gun = new CHKPlate(t.second->ip, t.second, pTaskMgr);
		gun->m_pDevMgr = this;
		plateDevList[gun->GetDeviceIP()] = gun;
		console->info("PLATE加载成功 名称:{}, IP:{}", t.second->camName, t.second->ip);
	}
	console->info("DH 列表 size:{}", dhCfgList.size());
	// 加载大华相机
	for (auto&t : dhCfgList)
	{
		console->info("Dh加载成功1 名称:{}, IP:{}", t.second->camName, t.second->ip);
		auto dh = new CDHCam(t.second->ip, t.second, pTaskMgr);
		dhDevList[dh->GetDeviceIP()] = dh;
		console->info("Dh加载成功2 名称:{}, IP:{}", t.second->camName, t.second->ip);
	}
	// 加载ICE车牌相机
	for (auto&t : iceCfgList)
	{
		auto gun = new CICEGun(t.second->ip, t.second, pTaskMgr);
		iceDevList[gun->GetDeviceIP()] = gun;
		console->info("ICE加载成功 名称:{}, IP:{}", t.second->name, t.second->ip);
	}
	// 加载吊具相机
	for (auto&t : AxisCfgList)
	{
		auto gun = new IPCamera(t.first, t.second, pTaskMgr);
		AxisDevList[t.first] = gun;
		console->info("AXIS加载成功 名称:{}, IP:{}", t.first, gun->GetDeviceIP());
	}

	// 线程: 相机登录，并启动相机相关线程
    thread login
    {
        [](CDeviceManager *pManager)
        {
			console->info("[线程][启动] 相机登录");

            // 海康相机
            for (auto&t : pManager->gunDevList)
            {
                if (t.second->LogIn())
                {
                    // 设备工作线程启动
                    console->debug("{} 初始化完成", t.first);
                }
                else
                {
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, t.second->GetInstallNmae() , "初始化失败",
						StateInfo_RUN_STATE_RUN_FAULT,t.second->GetDeviceIP());
                    console->error("{} 相机初始化失败", t.first);
					g_isOCRCamInitFail = true;
				}
				t.second->StartThreads();
            }
			for (auto&t : pManager->plateDevList)
			{
				if (t.second->LogIn())
				{
					// 设备工作线程启动
					console->debug("{} 初始化完成", t.first);
				}
				else
				{
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, t.second->GetInstallNmae(), "初始化失败",
						StateInfo_RUN_STATE_RUN_FAULT, t.second->GetDeviceIP());
					console->error("{} 相机初始化失败", t.first);
					g_isOCRCamInitFail = true;
				}
				t.second->StartThreads();
			}
			// 大华相机
			for (auto&t : pManager->dhDevList)
			{
				if (t.second->LogIn())
				{
					// 设备工作线程启动
					console->debug("{} 初始化完成", t.first);
				}
				else
				{
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, t.second->GetInstallNmae(), "初始化失败",
						StateInfo_RUN_STATE_RUN_FAULT, t.second->GetDeviceIP());
					console->error("{} 相机初始化失败", t.first);
					g_isOCRCamInitFail = true;
				}
				t.second->StartThreads();
			}
			//针孔相机
			for (auto&t : pManager->AxisDevList)
			{
				if (t.second->LogIn())
				{
					// 设备工作线程启动
					console->debug("{} 初始化完成", t.first);
				}
				else
				{
					PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, t.second->GetInstallNmae(), "初始化失败",
						StateInfo_RUN_STATE_RUN_FAULT, t.second->GetDeviceIP());
					console->error("{} 相机初始化失败", t.first);
					g_isOCRCamInitFail = true;
				}
				t.second->StartThreads();
			}
			//ICE相机在对象构造后即创建线程自动连接
			/*for (auto&t : pManager->iceDevList)
			{
				
			}*/

			console->info("[线程][结束] 相机登录");
        }, this
    };
    login.detach();

	// 线程: 检测相机运行状态
    worker = std::thread
    {
        [](CDeviceManager *pManager)
        {
			console->info("[线程][启动] 相机运行状态监控");

            while (!pManager->exitFlag)
            {
				console->info("开启一次相机状态监控");
				TimeMeter timer;
				timer.start();

                for (auto&t : pManager->gunDevList)
                {
					console->info("相机状态监控 {}", t.first);
                    t.second->RemoteControl();
					SleepMilliseconds(100);		// 检测相机状态的SDK连续调用时间隔一定时间，否则无法立刻检测出下一个相机离线
                }
				for (auto&t : pManager->AxisDevList)
				{
					console->info("相机状态监控 {}", t.first);
					t.second->RemoteControl();
				}

				console->info("结束一次相机状态监控，耗时 {}ms", timer.stop());

                SleepMilliseconds(15 * 1000);
            };

			console->info("[线程][结束] 相机运行状态监控");
        }, this
    };
	console->info("相机状态监控线程启动成功");
}

void CDeviceManager::Stop()
{
    for (auto&t : gunDevList)
    {
        t.second->StopThreads();
    }

    exitFlag = true;

    worker.detach();

    // listener.detach();

    console->info("设备管理模块退出");
}

void CDeviceManager::LoadDevCfg(string xml_path)
{
	try
	{
		console->info("Load {}", xml_path);
		std::string config_path = GetAppPath() + "\\" + xml_path;
		ptree xmlRootNode;
		read_xml(config_path, xmlRootNode);
		auto pRootChilds = xmlRootNode.get_child("GalaxyVism");

		for (auto&item : pRootChilds)
		{
			console->info("Load {}", item.first);
			if (_stricmp(item.first.c_str(), "HK") == 0)
			{
				console->info("Load {}", item.first);
				auto pChilds = item.second.get_child("");
				for (auto&child : pChilds)
				{
					if (child.first != "<xmlattr>" && child.first != "<xmlcomment>")
					{
						console->info("Load {} {}", item.first, child.first);
						HKGunConfig *pCfg = new HKGunConfig(child.first);
						if (pCfg->deserialize(child.first, child.second))
						{
							if (pCfg->usable)
							{
								gunCfgList[pCfg->ip] = pCfg;
							}
							else
							{
								console->info("Load {} {} {} not use", item.first, child.first, pCfg->ip);
								delete pCfg;
								pCfg = nullptr;
							}
						}
						else
						{
							console->error("Load {} {} fail", item.first, child.first);
							delete pCfg;
							pCfg = nullptr;
						}
						console->info("Load {} {} done", item.first, child.first);
					}
				}
			}
			if (_stricmp(item.first.c_str(), "DH") == 0)
			{
				console->info("DH Load {}", item.first);
				auto pChilds = item.second.get_child("");
				for (auto&child : pChilds)
				{
					if (child.first != "<xmlattr>" && child.first != "<xmlcomment>")
					{
						console->info("DH Load {} {}", item.first, child.first);
						DHCamConfig *pCfg = new DHCamConfig(child.first);
						if (pCfg->deserialize(child.first, child.second))
						{
							if (pCfg->usable)
							{
								dhCfgList[pCfg->ip] = pCfg;
							}
							else
							{
								console->info("DH Load {} {} {} not use", item.first, child.first, pCfg->ip);
								delete pCfg;
								pCfg = nullptr;
							}
						}
						else
						{
							console->error("DH Load {} {} fail", item.first, child.first);
							delete pCfg;
							pCfg = nullptr;
						}
						console->info("DH Load {} {} done", item.first, child.first);
					}
				}
			}
			else if (_stricmp(item.first.c_str(), "AxisF44") == 0)
			{
				console->info("Load {}", item.first);
				auto pChilds = item.second.get_child("");
				for (auto&child : pChilds)
				{
					if (child.first != "<xmlattr>" && child.first != "<xmlcomment>")
					{
						console->info("Load {} {}", item.first, child.first);
						AxisF44Config *pCfg = new AxisF44Config(child.first);
						if (pCfg->deserialize(child.first, child.second))
						{
							if (pCfg->usable)
							{
								AxisCfgList[child.first] = pCfg;
							}
							else
							{
								console->info("Load {} {} {} not use", item.first, child.first, pCfg->ip);
								delete pCfg;
								pCfg = nullptr;
							}
						}
						else
						{
							console->error("Load {} {} fail", item.first, child.first);
							delete pCfg;
							pCfg = nullptr;
						}
						console->info("Load {} {} done", item.first, child.first);
					}
				}
			}
			else if (_stricmp(item.first.c_str(), "ICE") == 0)
			{
				console->info("Load {}", item.first);
				auto pChilds = item.second.get_child("");
				for (auto&child : pChilds)
				{
					if (child.first != "<xmlattr>" && child.first != "<xmlcomment>")
					{
						console->info("Load {} {}", item.first, child.first);
						ICEConfig *pCfg = new ICEConfig(child.first);
						if (pCfg->deserialize(child.first, child.second))
						{
							if (pCfg->usable)
							{
								iceCfgList[pCfg->ip] = pCfg;
							}
							else
							{
								console->info("Load {} {} {} not use", item.first, child.first, pCfg->ip);
								delete pCfg;
								pCfg = nullptr;
							}
						}
						else
						{
							console->error("Load {} {} fail", item.first, child.first);
							delete pCfg;
							pCfg = nullptr;
						}
						console->info("Load {} {} done", item.first, child.first);
					}
				}
			}
			else if (_stricmp(item.first.c_str(), "HKPlate") == 0)
			{
				console->info("Load {}", item.first);
				auto pChilds = item.second.get_child("");
				for (auto&child : pChilds)
				{
					if (child.first != "<xmlattr>" && child.first != "<xmlcomment>")
					{
						console->info("Load {} {}", item.first, child.first);
						HKPlateConfig *pCfg = new HKPlateConfig(child.first);
						if (pCfg->deserialize(child.first, child.second))
						{
							if (pCfg->usable)
							{
								plateCfgList[pCfg->ip] = pCfg;
							}
							else
							{
								console->info("Load {} {} {} not use", item.first, child.first, pCfg->ip);
								delete pCfg;
								pCfg = nullptr;
							}
						}
						else
						{
							console->error("Load {} {} fail", item.first, child.first);
							delete pCfg;
							pCfg = nullptr;
						}
						console->info("Load {} {} done", item.first, child.first);
					}
				}
			}
		}
		console->info("Load {} done", xml_path);
	}
	catch (std::exception& e)
	{
		console->error("Load {} error:{}", xml_path, e.what());
		exit(1);
	}
	catch (...)
	{
		console->error("Load {} error", xml_path);
		exit(1);
	}
}

