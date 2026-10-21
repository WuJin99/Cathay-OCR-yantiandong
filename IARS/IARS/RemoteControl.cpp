#include "RemoteControl.h"

#include "PicSave.h"
#include "CGRPCRequest.h"
//#include "PlcComm.h"
//#pragma comment(lib, "PlcComm.lib")

void RecvSubMsg(OCRAny* pMsg)
{
	console->info("接收到消息：\n{}", ConvertChs(pMsg->DebugString()));
	if (pMsg->has_any())
	{
		OCRAny *pCopy = new OCRAny(*pMsg);
		g_queueSubMsg.add(pCopy);
	}
}

CRemoteControl::CRemoteControl()
    : exitFlag(false)
{

}

CRemoteControl::~CRemoteControl()
{
	NET_DVR_Cleanup();
	ICE_IPCSDK_Fini();
}

int CRemoteControl::Start()
{
	exitFlag = false;

	g_pPicSave = std::make_shared<PicSave>();
	g_pPicSave->Start();

	// ZMQ管理模块
	try
	{
		boost::shared_ptr<ZMQManager> zmq(new ZMQManager());
		zmqManagerCtrl = zmq;
		if (!zmqManagerCtrl->Start())
		{
			if (!zmqManagerCtrl->GetIsPubOk())
			{
				//设置pub队列在连接失败时不添加数据，防止内存泄露
				//g_WaitPubMsg.setIsAdd(zmqManagerCtrl->GetIsPubOk());
				console->error("ZMQ Pub 启动 失败");

			}
			if (!zmqManagerCtrl->GetIsSubOk())
			{
				console->error("ZMQ Sub 启动 失败");
			}
			console->error("ZMQ管理模块启动 失败");
		}
	}
	catch (std::exception& e)
	{
		console->error("ZMQ管理模块启动 失败 {}", e.what());
	}
	catch (...)
	{
		console->error("ZMQ管理模块启动 失败");
		// return -1;
	}


	//boost::shared_ptr<TaskManager> pTaskMgr = boost::make_shared<TaskManager>();
	// 任务管理模块
    try
    {
		for (auto& t : g_AppConfig.arrLaneSide)
		{
			boost::shared_ptr<TaskManager> pMgr = boost::make_shared<TaskManager>();
			pMgr->Start(t.name, t.lane_id, t.cfg);
			t.pTaskMgr = pMgr;
			pTaskMgr = pMgr;
		}
	}
	catch (std::exception& e)
	{
		console->error("任务管理模块启动 失败 {}", e.what());
	}
    catch (...)
    {
        console->error("任务管理模块启动 失败");
        return -1;
    }

	// 调试模块
    try
    {
        debugCtrl = boost::make_shared<CDebug>();
        debugCtrl->Start();
        console->debug("调试模块启动 成功");
	}
	catch (std::exception& e)
	{
		console->error("调试模块启动 失败 {}", e.what());
	}
    catch (...)
    {
        console->error("调试模块启动 失败");
        //return -1;
    }

	for (auto&sub : g_AppConfig.arrSubCfg)
	{
		CZmqComm *pComm = new CZmqComm;
		sub.pObj = pComm;
		pComm->SetRecvHandler(RecvSubMsg);
		if (pComm->Start(sub.ipaddr, sub.port, ZMQ_SUB))
		{
			console->info("初始化SUB成功 {}:{}", sub.ipaddr, sub.port);
		}
		else
		{
			console->error("初始化SUB失败 {}:{} err:{}", sub.ipaddr, sub.port, pComm->GetErrMsg());
		}
	}
	CGRPCRequest::GetInstance()->Start();
	CRecodData::GetInstance()->Start();
	// PLC 通讯模块
    try
	{
		plcCtrl = boost::make_shared<PLCHttpComm>(pTaskMgr);
		plcCtrl->Start();
		/*auto PlcReadBack = [](unsigned char* pBuf, int nLen, void*pParam){
			CRemoteControl *pThis = (CRemoteControl*)pParam;
			if (pThis)
				pThis->DealPlcData(pBuf, nLen);
		};
		PCI_SetReadBufCallback(PlcReadBack, this);*/
		/*if (!PCI_Init(console, "./AppConfig.xml", PLC_MESSAGE_SIZE, OCR_TO_PLC_MESSAGE_SIZE))
		{
			console->error("PLC通讯模块启动 失败");
			PackStateInfo(StateInfo_ITEM_TYPE_ITEM_PLC, "PLC", "PLC通讯模块启动 失败",
				StateInfo_RUN_STATE_RUN_FAULT, "");
		}
		else
		{
			console->info("PLC通讯模块启动 成功");
		}*/
	}
	catch (std::exception& e)
	{
		PackStateInfo(StateInfo_ITEM_TYPE_ITEM_PLC, "PLC", e.what(),
			StateInfo_RUN_STATE_RUN_FAULT, "");
		console->error("PLC通讯模块启动 失败 {}", e.what());
	}
    catch (...)
    {
        console->error("PLC通讯模块启动 失败");
		PackStateInfo(StateInfo_ITEM_TYPE_ITEM_PLC, "PLC", "PLC通讯模块启动 失败",
			StateInfo_RUN_STATE_RUN_FAULT, "");
        //return -1;
    }
	if(g_OcrSystemState!=OCR_CRS_DISCONNECT)
	g_OcrSystemState = OCR_SYS_NORMAL;


    return 0;
}

void CRemoteControl::Stop()
{
	exitFlag = true;

	if (debugCtrl)
	{
		debugCtrl->Stop();
	}

	if (zmqManagerCtrl)
	{
		zmqManagerCtrl->Stop();
	}
	//PCI_UnInit();
	if (plcCtrl)
	{
		plcCtrl->Stop();
	}

    SleepMilliseconds(5 * 1000);
}

void CRemoteControl::DealPlcData(unsigned char* pBuf, int nLen)
{
	//PLCMESSAGE plc_msg;
	//memcpy(plc_msg.data, pBuf, nLen);
	//plc_msg.rtime = time(nullptr);
	//PLCInfo plc_info;
	//plc_info.ParseMsg(plc_msg);
	//for (auto& t : g_plcSubscriber)
	//{
	//	if (t.second)
	//	{
	//		*t.second = plc_info;
	//	}
	//}
	//// 通知PLC数据更新
	//boost::mutex::scoped_lock lock(g_PlcUpdateMutex);
	//g_PlcUpdateCond.notify_all();

	//unsigned char szWriteBuf[100] = {};
	////每写一次，消息编号自增1
	//g_mtxOCRToPlc.lock();
	//g_OCRToPlc.usMsgNumber++;
	//string strWritePlc = g_OCRToPlc.to_string();
	//int nWriteLen = g_OCRToPlc.SeriralToBuffer(szWriteBuf);
	//g_mtxOCRToPlc.unlock();
	//PCI_SetWriteBuf(szWriteBuf, nWriteLen);
	//
	//bool bIsShow = PCI_IsShow();
	//if (bIsShow)
	//{
	//	console->info("{}", plc_info);
	//	console->debug("{}", strWritePlc);
	//}
}
