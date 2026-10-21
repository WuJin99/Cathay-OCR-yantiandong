#include "HCNetSDK.h"
//#include <curl/curl.h>
#include "Console.h"
#include "AppConfig.h"
#include "CameraConfig.h"
#include "RemoteControl.h"
#include "CrashDetect.h"
//#include "cJSON.h"
#ifdef _MSC_VER
// win64
#pragma comment(lib, "HCNetSDK.lib")

#pragma comment(lib, "dhnetsdk.lib")//dh

#pragma comment(lib, "PlayCtrl.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "libzmq.lib")
#pragma comment(lib, "libcurl.lib")
#ifdef _DEBUG
#pragma comment(lib, "libboost_filesystem-vc141-mt-gd-x64-1_69.lib")
#pragma comment(lib, "opencv_world343d.lib")
#pragma comment(lib, "libprotobufd.lib")
#pragma comment(lib, "libzmq-v120-mt-gd-4_3_2.lib")
#else
#pragma comment(lib, "libboost_filesystem-vc141-mt-x64-1_69.lib")
#pragma comment(lib, "opencv_world343.lib")
#pragma comment(lib, "libprotobuf.lib")
#pragma comment(lib, "libzmq-v120-mt-4_3_2.lib")
#endif
#elif __linux__
// linux

#elif __ARM__
#endif 

bool CtrlHandler(DWORD dwType)
{
	switch (dwType)
	{
		// handle the ctrl-c signal.
	case CTRL_C_EVENT:
		console->info("*************** ctrl-c event ***************\n\n");
		std::cin.clear();
		// 忽略缓冲区中的错误字符
		std::cin.ignore(1000, '\n');
		return(true);
		// ctrl-close: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT:
		console->info("*************** ctrl-close event ***************\n\n");
		return(true);
		// pass other signals to the next handler.
	case CTRL_BREAK_EVENT:
		console->info("*************** ctrl-break event ***************\n\n");
		return false;
	case CTRL_LOGOFF_EVENT:
		console->info("*************** ctrl-logoff event ***************\n\n");
		return false;
	case CTRL_SHUTDOWN_EVENT:
		console->info("*************** ctrl-shutdown event ***************\n\n");
		return false;
	default:
		return false;
	}
}

int main()
{
	//{
	//	cJSON *root = cJSON_CreateObject();
	//	cJSON_AddTrueToObject(root, "IsSuccess");
	//	cJSON_AddStringToObject(root, "Message", "OK");

	//	cJSON *data = cJSON_CreateObject();
	//	cJSON_AddItemToObject(root, "Data", data);
	//	cJSON_AddStringToObject(data, "UUID", GetUUID().c_str());
	//	cJSON_AddStringToObject(data, "Timestamp", GetHttpTimeString().c_str());
	//	cJSON_AddStringToObject(data, "TaskId", "");
	//	cJSON_AddStringToObject(data, "CraneId", "岸桥123");
	//	cJSON_AddNumberToObject(data, "LandNo", 1.2);

	//	
	//	string  json_str = cJSON_Print(root);
	//	//cJSON_Delete(data);
	//	cJSON_Delete(root);
	//	return 0;
	//}
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdIn)
	{
		DWORD dwMode;
		GetConsoleMode(hStdIn, &dwMode);
		dwMode &= ~ENABLE_QUICK_EDIT_MODE;
		dwMode &= ~ENABLE_INSERT_MODE;
		SetConsoleMode(hStdIn, dwMode);
	}
	SetUnhandledExceptionFilter(ApplicationCrashHandler);
    // curl 初始化
    //curl_global_init(CURL_GLOBAL_ALL);
	
	//日志初始化
	std::string logPath = GetAppPath() + "\\ConsoleLog\\";
	MakeDirs(logPath);
	InitConsole(logPath);
	//单进程处理
	HANDLE hMutex = CreateMutex(NULL, TRUE, "IARS/8774-P2204");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		//关闭信号量句柄
		CloseHandle(hMutex);

		console->info("采集程序已运行");
		std::this_thread::sleep_for(std::chrono::seconds(3));

		return 0;
	}

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    // 初始化AppConfig.ml配置文件
    std::string AppCfgPath = GetAppPath() + "\\AppConfig.xml";
    LoadAppConfigXML(AppCfgPath);
    console->info("xml 初始化");

    CRemoteControl remotrControl;
    if (remotrControl.Start())
    {
        isExit = true;
    }

	shared_ptr<tMainLog> pLog;
	time_t tCur = time(nullptr);
	g_StateInfo.set_dev_id(g_AppConfig.userCfg.quayNo);
    while (!isExit)
    {
		//每5秒写入一次日志
        SleepMilliseconds(5000);
		while (g_queueLog.get(pLog))
		{
			FILE *pFile = nullptr;
			fopen_s(&pFile, pLog->path.c_str(), "a+t");
			if (pFile)
			{
				fwrite(pLog->time.c_str(), 1, pLog->time.length(), pFile);
				fwrite(pLog->info.c_str(), 1, pLog->info.length(), pFile);
				fwrite("\n", 1, 1, pFile);
				fclose(pFile);
				pFile = nullptr;
			}
		}
		for (auto&mgr:g_AppConfig.arrLaneSide)
		{
			mgr.pTaskMgr->SaveLogToFile();
		}
		if (g_AppConfig.userCfg.stateInfoInterval > 0 &&
			time(nullptr) - tCur >= g_AppConfig.userCfg.stateInfoInterval)
		{
			tCur = time(nullptr);
			std::lock_guard<std::mutex> lck(g_mtxStateInfo);
			if (g_StateInfo.items_size())
			{
				//组包
				OCRAny any;
				any.mutable_any()->PackFrom(g_StateInfo);
				any.set_msg_id(GetUUID());
				any.set_msg_type(MSG_TYPE::MSG_STATEINFO);
				any.set_src_id(g_AppConfig.userCfg.IARSName);
				any.set_time(GetTimeString());
				//any.set_need_reply(1);
				any.set_dev_id(g_AppConfig.userCfg.quayNo);
				g_WaitPubMsg.add(any);
				auto dbg_str = console->warn("上传状态信息 StateInfo:\n{}", ConvertChs(any.DebugString()));
				if (StateInfo_RUN_STATE_RUN_NORMAL != g_StateInfoType)
				{
					//状态为不正常时，记录到日志
					AddMainLog(dbg_str);
				}
				g_StateInfoType = StateInfo_RUN_STATE_RUN_NORMAL;
				g_StateInfo.clear_items();
			}
		}
    }
    remotrControl.Stop();

    console->error("主线程 结束");
    return 0;
}