#include "ZMQManager.h"
#include "AppConfig.h"

#include "SubManager.h"
#include "Monitor.h"
ZMQManager::ZMQManager()
	: m_bRun(false)
	, isPubOk(false)
	, isSubOk(false)
{
	//创建文件，缓存待应答的消息
	savePath = GetAppPath() + "\\RePub\\";
	MakeDirs(savePath);

	//读取本地未发送消息
	vector<std::string>names;
	GetFilesNames(savePath, names);
	for (auto &a : names)
	{

		OCRAny any;
		ReadLocalFileToProto(savePath + a, any);

		g_WaitPubMsg.add(any);
		console->warn("本地读取待发送消息：msg:{}", ConvertChs(any.DebugString()));
	}
}

ZMQManager::~ZMQManager()
{
}

bool ZMQManager::GetIsPubOk()
{
	return isPubOk;
}
bool ZMQManager::GetIsSubOk()
{
	return isSubOk;
}
bool ZMQManager::Start()
{
	//sub初始化
	isSubOk = SubManager::GetInstance()->InitSubList(g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.sub_port);
	if (isSubOk)
		console->info("Init SUB {}:{} ok", g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.sub_port);
	else
		console->error("Init SUB {}:{} fail", g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.sub_port);
	//Xpub初始化
	isPubOk = xPubZmq.Start(g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.xpub_port, ZMQ_XPUB);
	if (isPubOk)
		console->info("Init XPUB {}:{} ok", g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.xpub_port);
	else
		console->error("Init XPUB {}:{} fail", g_AppConfig.brokerCfg.IP, g_AppConfig.brokerCfg.xpub_port);
	if (!(isPubOk && isSubOk))
	{
		console->error("[ZMQ] start false. pub:{}  sub:{}", isPubOk,isSubOk);
		return false;
	}
	//首先主动发送一个心跳数据，让服务端回复
	OCRAny any;
	any.set_msg_id(GetUUID());
	any.set_msg_type(MSG_TYPE::MSG_HEART_BEAT);
	any.set_src_id(g_AppConfig.userCfg.IARSName);
	any.set_time(GetTimeString());
	any.set_need_reply(1);//需要应答
	any.set_dev_id(g_AppConfig.userCfg.quayNo);
	xPubZmq.XPubMsg(any);

	//发送车牌/车号列表请求
	gui::pb::TruckListRequest request;
	gui::pb::Header *pHeader = request.mutable_header();
	pHeader->set_crane(g_AppConfig.userCfg.quayNo);
	pHeader->set_messageid(GetUUID());
	pHeader->set_timestamp(GetTimeString());
	any.set_msg_id(GetUUID());
	any.set_msg_type(MSG_TYPE::MSG_TruckList);
	any.set_src_id(g_AppConfig.userCfg.IARSName);
	any.set_time(GetTimeString());
	any.set_need_reply(1);//需要应答
	any.set_dev_id(g_AppConfig.userCfg.quayNo);
	any.mutable_any()->PackFrom(request);
	xPubZmq.XPubMsg(any);

	//设置服务为可用，并更新超时时间
	m_ServerRun = true;
	GetCurrentStamp64(rrTick);
	//开始：发送线程、接收线程和心跳线程
	m_bRun = true;
	//心跳
	std::thread heartBeatThread = std::thread
	{
		[&](ZMQManager *zmq)
		{
			time_t tHeartBeat = time(nullptr);
			time_t tStateInfo = time(nullptr);
			while (zmq->m_bRun)
			{
				if (time(nullptr) - tHeartBeat >= g_AppConfig.userCfg.heartBeatInterval)
				{
					tHeartBeat = time(nullptr);
					OCRAny any;
					any.set_msg_id(GetUUID());
					any.set_msg_type(MSG_TYPE::MSG_HEART_BEAT);
					any.set_src_id(g_AppConfig.userCfg.IARSName);
					any.set_time(GetTimeString());
					any.set_need_reply(1);//需要应答
					any.set_dev_id(g_AppConfig.userCfg.quayNo);
					zmq->xPubZmq.XPubMsg(any);
				}
				if (g_AppConfig.userCfg.stateInfoInterval > 0 &&
					time(nullptr) - tStateInfo >= g_AppConfig.userCfg.stateInfoInterval)
				{
					tStateInfo = time(nullptr);
					zmq->DealStateInfo();
				}
				zmq->DealXPub();
				zmq->DealSub();
				SleepMilliseconds(100);
			};
		}, this
	};
	heartBeatThread.detach();

	return true;
}

void ZMQManager::Stop()
{
	m_bRun = false;
}
bool ZMQManager::IsTimeOut()
{
	int64_t tTimeOutTick = GetCurrentStamp64();
	return (tTimeOutTick - rrTick) >= (g_AppConfig.userCfg.heartBeatTimeout * 1000);
}

void ZMQManager::DealSub()
{
	OCRAny *pMsg = nullptr;
	boost::mutex::scoped_lock lock(m_NeedReplyMsgQueMutex);
	while (g_queueSubMsg.size() > 0)
	{
		if (g_queueSubMsg.get(pMsg))
		{
			if (pMsg->msg_type() == MSG_TYPE::MSG_CONFIRMREPLY)
			{
				//收到回复，服务端设置可用，并更新超时时间
				m_ServerRun = true;
				GetCurrentStamp64(rrTick);

				std::shared_ptr<ConfirmReply> rep(new ConfirmReply);
				pMsg->any().UnpackTo(rep.get());
				//重发
				if (rep->type() == ConfirmReply_REPLY_TYPE_REPLY_RESEND)//重发
				{
					console->warn("接收到消息回复，重发。回复的消息msg_id:{}", rep->msg_id());

					//重新加到待发送队列里面
					/*for (auto iter = zmq->pub.m_NeedReplyMsgQue.begin(); iter != zmq->pub.m_NeedReplyMsgQue.end(); ++iter)
					{
						if (isEqual(iter->id, rep->msg_id()))
						{
							g_WaitPubMsg.add(iter->any);
							break;
						}
					}*/
				}
				//接收成功
				else if (rep->type() == ConfirmReply_REPLY_TYPE_REPLY_SUCC)//接收成功
				{
					if (g_AppConfig.userCfg.sendInfoShow > 1)
					{
						console->info("接收到消息回复，成功。回复的消息msg_id:{}", rep->msg_id());
					}
					//删除管理队列里面数据
					bool isFind = false;
					for (auto iter = m_NeedReplyMsgQue.begin(); iter != m_NeedReplyMsgQue.end(); ++iter)
					{
						if (isEqual(iter->id, rep->msg_id()))
						{
							isFind = true;
							m_NeedReplyMsgQue.erase(iter);
							//删除本地保存的消息
							if (RemoveLocalFile(savePath + rep->msg_id()) == 0)
							{
								console->error("删除本地保存的消息失败 msg_id:{}", rep->msg_id());
							}
							else {
								console->info("删除本地保存的消息成功 msg_id:{}", rep->msg_id());
							}
							break;
						}
					}
					if (!isFind)
					{
						console->warn("管理队列未找到本条数据 msg_id:{}", rep->msg_id());
					}
				}
			}
			else if (pMsg->any().Is<gui::pb::TruckListReply>())
			{
				gui::pb::TruckListReply reply;
				if (pMsg->any().UnpackTo(&reply))
				{
					console->info("接收到TruckListReply \n{}", ConvertChs(reply.DebugString()));
					//将worklist存入根路径下
					string save_path = GetAppPath() + "\\TruckList.txt";
					WriteStringToTxt(save_path, ConvertChs(pMsg->DebugString()));

					g_TruckListReply = reply;
				}
				else
				{
					string save_path = GetAppPath() + "\\TruckList.txt";
					WriteStringToTxt(save_path, console->error("解析TruckListReply失败 msg_id:{}", pMsg->msg_id()));
				}
			}
			if (pMsg->any().Is<ocrs::pb::THPMResult>())
			{
				ocrs::pb::THPMResult ret;
				pMsg->any().UnpackTo(&ret);
				for (auto&lane : g_AppConfig.arrLaneSide)
				{
					lane.pTaskMgr->SetTruckResult(ret);
				}
			}
			if (pMsg->any().Is<ocrs::pb::TRUCKSTATUS>())
			{
				ocrs::pb::TRUCKSTATUS ret;
				pMsg->any().UnpackTo(&ret);
				for (auto&lane : g_AppConfig.arrLaneSide)
				{
					lane.pTaskMgr->SetTruckStatus(ret);
				}

			}
			delete pMsg;
			pMsg = nullptr;
		}
		else
		{
			if (IsTimeOut())
			{
				//如果一段时间收取不到服务端的回复信息或者心跳
				m_ServerRun = false;
			}
		}

	}
	//待回复的消息超时重发，重发次数超过配置的次数后，删除
	for (auto iter = m_NeedReplyMsgQue.begin(); iter != m_NeedReplyMsgQue.end(); )
	{
		//大于重复发送的次数，删除。设置重发次数为0时没有重发
		if (iter->count >= g_AppConfig.brokerCfg.resend_count)
		{
			console->warn("[PubManager] 重复发送消息次数超限，删除，id:{}   msg:{}", iter->id, iter->any.DebugString());
			//删除本地保存的消息
			if (RemoveLocalFile(savePath + iter->id) == 0)
			{
				console->error("删除消息失败 msg_id:{}", iter->id);
			}
			//删除管理队列数据
			m_NeedReplyMsgQue.erase(iter);
			break;
		}
		else
		{
			if (GetCurrentStamp64() - iter->timeMillSeconds > g_AppConfig.userCfg.needReplyResend * 1000)
			{
				//发送次数加1，更新时间，不更新时间会每次都发送
				iter->count += 1;
				iter->timeMillSeconds = GetCurrentStamp64();
				console->info("[PubManager] 再次发送消息，count:{}  id:{}   msg:{}", iter->count, iter->id, iter->any.DebugString());
				//发送可用才添加到待发送队列
				if (m_ServerRun)
				{
					xPubZmq.XPubMsg(iter->any);
				}
			}

			++iter;
		}
	}
}

void ZMQManager::DealXPub()
{
	OCRAny any;
	while (g_WaitPubMsg.get(any))
	{
		//服务端不可用时，不需要应答的消息不发送，避免在消息代理堆积
		if (any.need_reply() != 1 && !m_ServerRun)
		{
			if (g_AppConfig.userCfg.sendInfoShow > 1)
			{
				console->warn("服务器不可用，消息发送失败:{}", ConvertChs(any.DebugString()));
			}
			continue;
		}
		if (m_ServerRun || any.msg_type() == MSG_TYPE::MSG_HEART_BEAT)
		{
			xPubZmq.XPubMsg(any);
		}
		if (1 == any.need_reply() && any.msg_type() != MSG_TYPE::MSG_HEART_BEAT)
		{
			if (g_AppConfig.userCfg.sendInfoShow > 1)
			{
				console->warn("待回复消息缓存:{}", ConvertChs(any.DebugString()));
			}
			//标识是否找到
			bool isHave = false;
			//线程同步
			boost::mutex::scoped_lock lock(m_NeedReplyMsgQueMutex);
			for (auto iter = m_NeedReplyMsgQue.begin(); iter != m_NeedReplyMsgQue.end(); ++iter)
			{
				if (isEqual(iter->id, any.msg_id()))
				{
					isHave = true;
					break;
				}
			}
			//管理队列中没有才添加
			if (!isHave)
			{
				m_NeedReplyMsgQue.push_back(needReplyMsgStruct{ any.msg_id(),any,0, GetCurrentStamp64() });
				WriteProtoToLocalFile(savePath + any.msg_id(), any);
			}
		}
	}
}

void ZMQManager::DealStateInfo()
{
	StateInfo::RUN_STATE type = StateInfo_RUN_STATE_RUN_NORMAL;
	//CPU 内存 GPU
	string info;
	double cpuRate = Monitor::GetCpuRate();
	std::stringstream ssCpu;
	ssCpu << std::setiosflags(std::ios::fixed) << std::setprecision(1) << cpuRate;
	std::string strCpu = ssCpu.str();

	if (cpuRate > g_AppConfig.monitorCfg.CPUCfg)
	{
		type = StateInfo_RUN_STATE_RUN_WARN;
		info += "CPU使用率：" + strCpu + "% > " + to_string(g_AppConfig.monitorCfg.CPUCfg) + "%;";
	}
	else
	{
		info += "CPU使用率：" + strCpu + "%;";
	}

	double memRate = Monitor::GetMemoryRate();
	std::stringstream ssmem;
	ssmem << std::setiosflags(std::ios::fixed) << std::setprecision(1) << memRate;
	std::string strmem = ssmem.str();

	if (memRate > g_AppConfig.monitorCfg.memCfg)
	{
		type = StateInfo_RUN_STATE_RUN_WARN;
		info += "内存使用率：" + strmem + "% > " + to_string(g_AppConfig.monitorCfg.memCfg) + "%;";
	}
	else
	{
		info += "内存使用率：" + strmem + "%;";
	}

	float gpuRate = 0;
	/*std::string strGpu;
	if (Monitor::GetGpuRate(gpuRate))
	{
		std::stringstream ssGpu;
		ssGpu << std::setiosflags(std::ios::fixed) << std::setprecision(1) << gpuRate;
		strGpu = ssGpu.str();

		if (gpuRate > g_AppConfig.monitorCfg.GPUCfg)
		{
			type = StateInfo_RUN_STATE_RUN_WARN;
			info += "GPU使用率：" + strGpu + "% > " + to_string(g_AppConfig.monitorCfg.GPUCfg) + "%;";
		}
		else
		{
			info += "GPU使用率：" + strGpu + "%;";
		}
	}
	else
	{
		strGpu = "error";
		info += "GPU使用率:获取失败;";
		type = StateInfo_RUN_STATE_RUN_WARN;
	}*/

	int nTotalG, nFreeG;
	if (GetDiskSpace(g_AppConfig.userCfg.savePath, nTotalG, nFreeG))
	{
		if (nFreeG*100.0 / nTotalG < g_AppConfig.monitorCfg.diskCfg)
		{
			type = StateInfo_RUN_STATE_RUN_WARN;
			info += "磁盘总存储： " + to_string(nTotalG) + "G;";
			info += "磁盘剩余存储： " + to_string(nFreeG) + "G 不足 " + to_string(g_AppConfig.monitorCfg.diskCfg) + "%;";
		}
		else
		{
			info += "磁盘总存储： " + to_string(nTotalG) + "G;";
			info += "磁盘剩余存储： " + to_string(nFreeG) + "G;";
		}
	}
	else
	{
		type = StateInfo_RUN_STATE_RUN_FAULT;
		info += "磁盘总存储：检测磁盘空间失败;";
	}

	PackStateInfo(StateInfo_ITEM_TYPE_ITEM_COMPUTER, "工控机", info, type, GetLocalIp(), strCpu, strmem);
}
