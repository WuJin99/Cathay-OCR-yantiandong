#ifndef SUBMANAGER_H
#define SUBMANAGER_H
#include "Console.h"
#include "ZmqComm.h"
#include "Utils.h"
#include "TaskManager.h"

void SubMsgParser(OCRAny *poa)
{
	console->warn("接收到消息12：\n{}", ConvertChs(poa->DebugString()));
	//从消息代理接收到ECS触发JobTrigger消息，直接进行处理
	if (poa->any().Is<gui::pb::JobTrigger>())
	{
		console->warn("接收到JobTrigger消息：\n{}", ConvertChs(poa->DebugString()));
		gui::pb::JobTrigger jt;
		if (poa->any().UnpackTo(&jt))
		{
			for (auto& t : g_AppConfig.arrLaneSide)
			{
				if (t.lane_id == jt.body().lane_id())
				{
					t.pTaskMgr->SetJobTrigger(jt);
					break;
				}
			}
		}
	}
	else
	{
		console->warn("接收到消息1：\n{}", ConvertChs(poa->DebugString()));
		OCRAny *pCopy = new OCRAny(*poa);
		g_queueSubMsg.add(pCopy);
	}
}
class SubManager
{
public:
    bool InitSubList(std::string ip, int port)
    {
		console->warn("接收到消息11：\n {}  {}", ip,port);
        CZmqComm *zmq = new CZmqComm;
		string subStr = g_AppConfig.userCfg.quayNo + ":svr:";
		zmq->SetSubFilter(subStr);
		//内部设置了消息订阅条件
		if (!zmq->Start(ip, port, ZMQ_SUB))
		{
			console->warn("接收到消息1---------1：\n {}  {}", ip, port);
			return false;
		}
		else
		{
			console->warn("接收到消息111111111：\n {}  {}", ip, port);
			zmq->SetRecvHandler(SubMsgParser);
			return true;
		}
    }

    static SubManager* GetInstance()
    {
		if (pSubManager == NULL)
		{
			pSubManager = new SubManager();
		}
        return pSubManager;
    }
private:

    SubManager()
    {
    }
    static SubManager *pSubManager;

};

SubManager* SubManager::pSubManager = NULL;

#endif //SUBMANAGER_H