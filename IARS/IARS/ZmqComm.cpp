#include "Utils.h"
#include "ZmqComm.h"
#include "Console.h"
#include "TimeMeter.hpp"
#include "AppConfig.h"

CZmqComm::CZmqComm()
    : m_ctx(nullptr)
    , m_socket(nullptr)
    , m_bRun(nullptr)
    , m_pFunc(nullptr)
    , m_bCanPub(false)
    , id_size(sizeof(id))
    , id_size_r(sizeof(id_r))
{
    m_msgQueue.setfunc(&CZmqComm::ZmqMsgClose);
}

CZmqComm::~CZmqComm()
{
    m_bCanPub = false;
    m_bRun = false;
    if (m_ctx)
    {
        zmq_ctx_shutdown(m_ctx);
        zmq_ctx_term(m_ctx);
        m_ctx = nullptr;
    }
}

bool CZmqComm::Start(string ip, int port, int type)
{
	if (m_ctx)
	{
		return true;
	}
    if (ZMQ_SUB == type)
    {
        m_ctx = zmq_ctx_new();
        m_socket = zmq_socket(m_ctx, ZMQ_SUB);
        if (!m_socket)
        {
            m_err_msg = zmq_strerror(zmq_errno());
            return false;
        }
        char szAddr[100] = {};
        sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);
        // 连接到服务器
        int ret = zmq_connect(m_socket, szAddr);
        if (ret != 0)
        {
            m_err_msg = zmq_strerror(zmq_errno());
            zmq_close(m_socket);
            zmq_ctx_term(m_ctx);
            m_ctx = nullptr;
            return false;
        }
        
		// 订阅所有消息
        //ret = zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);
		ret = zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, m_strSubFilter.c_str(), m_strSubFilter.length());
        m_bRun = true;
        boost::thread_group tg;
        tg.create_thread(boost::bind(&CZmqComm::SubThread, this));
        console->debug("[ZMQ] 连接URL {} 成功", szAddr);
        return true;
    }
    else if (ZMQ_PUB == type)
    {
        m_ctx = zmq_ctx_new();
        m_socket = zmq_socket(m_ctx, ZMQ_PUB);
        if (!m_socket)
        {
            m_err_msg = zmq_strerror(zmq_errno());
            return false;
        }
        char szAddr[100] = {};
        sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);
        // 连接到服务器
        int ret = zmq_bind(m_socket, szAddr);
        if (ret != 0)
        {
            m_err_msg = zmq_strerror(zmq_errno());
            zmq_close(m_socket);
            zmq_ctx_term(m_ctx);
            m_ctx = nullptr;
            return false;
        }
        m_bRun = true;
        m_bCanPub = true;
        boost::thread_group tg;
        tg.create_thread(boost::bind(&CZmqComm::PubThread, this));
        console->debug("[ZMQ] 连接URL {} 成功", szAddr);
        return true;
    }
    else if (ZMQ_REQ == type)
    {
        m_ctx = zmq_ctx_new();
        if (!m_ctx)
        {
            console->warn("[ZMQ] ZMQ创建上下文失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }

        m_socket = zmq_socket(m_ctx, ZMQ_REQ);
        if (!m_socket)
        {
            console->warn("[ZMQ] ZMQ创建socket失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }

        char szAddr[100] = {};
        sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);
        console->debug("[ZMQ] 连接URL: {}", szAddr);

        int rc = zmq_connect(m_socket, szAddr);
        if (rc != 0)
        {
            console->warn("[ZMQ] ZMQ connect 失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }
        m_bRun = true;
        boost::thread_group tg;
        tg.create_thread(boost::bind(&CZmqComm::ReqThread, this));
        console->debug("[ZMQ] 连接URL {} 成功", szAddr);
        return true;
    }
    else if (ZMQ_REP == type)
    {
        m_ctx = zmq_ctx_new();
        if (!m_ctx)
        {
            console->warn("[ZMQ] ZMQ创建上下文失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }
        m_socket = zmq_socket(m_ctx, ZMQ_REP);
        if (!m_socket)
        {
            console->warn("[ZMQ] ZMQ创建socket失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }

        char szAddr[100] = {};
        sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);

        int rc = zmq_bind(m_socket, szAddr);
        if (rc == -1)
        {
            console->warn("[ZMQ] ZMQ zmq_bind 失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }
        m_bRun = true;
        boost::thread_group tg;
        tg.create_thread(boost::bind(&CZmqComm::RepThread, this));
        console->debug("[ZMQ] 连接URL {} 成功", szAddr);
        return true;
    }
    else if (ZMQ_STREAM == type)
    {
        m_ctx = zmq_ctx_new();
        if (!m_ctx)
        {
            console->warn("[ZMQ] ZMQ创建上下文失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }
        m_socket = zmq_socket(m_ctx, ZMQ_STREAM);
        if (!m_socket)
        {
            console->warn("[ZMQ] ZMQ创建socket失败: {}", zmq_strerror(zmq_errno()));
            return false;
        }

        int timeo = 500;
        int enabled = 1;
        zmq_setsockopt(m_socket, ZMQ_LINGER, &timeo, sizeof(timeo));
        zmq_setsockopt(m_socket, ZMQ_SNDTIMEO, &timeo, sizeof(timeo));
        zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeo, sizeof(timeo));
        zmq_setsockopt(m_socket, ZMQ_STREAM_NOTIFY, &enabled, sizeof(enabled));

        char szAddr[100] = {};
        sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);

        int rc = zmq_connect(m_socket, szAddr);
        if (rc == -1)
        {
            int en = zmq_errno();
            console->warn("[ZMQ] ZMQ connect 失败: {}", zmq_strerror(en));
            return -1;
        }
        zmq_getsockopt(m_socket, ZMQ_IDENTITY, id, &id_size);

        m_bRun = true;
        boost::thread_group tg;
        tg.create_thread(boost::bind(&CZmqComm::RecvThread, this));

        console->debug("[ZMQ] 连接URL {} 成功", szAddr);
        return true;
    }
	else if (ZMQ_XPUB == type)
	{
		m_ctx = zmq_ctx_new();
		if (!m_ctx)
		{
			console->warn("[ZMQ] ZMQ创建上下文失败: {}", zmq_strerror(zmq_errno()));
			return false;
		}
		m_socket = zmq_socket(m_ctx, ZMQ_XPUB);
		if (!m_socket)
		{
			console->warn("[ZMQ] ZMQ创建socket失败: {}", zmq_strerror(zmq_errno()));
			return false;
		}

		char szAddr[100] = {};
		sprintf_s(szAddr, "tcp://%s:%d", ip.c_str(), port);

		int rc = zmq_connect(m_socket, szAddr);
		if (rc == -1)
		{
			int en = zmq_errno();
			console->warn("[ZMQ] ZMQ connect 失败: {}", zmq_strerror(en));
			return -1;
		}

		m_bRun = true;
		boost::thread_group tg;
		tg.create_thread(boost::bind(&CZmqComm::XPubThread, this));

		console->debug("[ZMQ] 连接URL {} 成功", szAddr);
		return true;
	}
	else
	{
		console->debug("[ZMQ] 位置类型");
	}
	return false;
}

void CZmqComm::Stop()
{
    m_bCanPub = false;

    m_bRun = false;

    if (m_ctx)
    {
        zmq_ctx_shutdown(m_ctx);
        zmq_ctx_term(m_ctx);

        m_ctx = nullptr;
    }
}

void CZmqComm::SendMsg(const google::protobuf::Message& promsg)
{
    OCRAny ocrAny;
    //ocrAny.set_msg_id(boost::posix_time::to_time_t(boost::posix_time::microsec_clock::local_time()));
    /*google::protobuf::Any *any = ocrAny.mutable_any();
    any->PackFrom(promsg);*/

    int msg_size = int(ocrAny.ByteSizeLong());
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, msg_size);
    ocrAny.SerializeToArray(zmq_msg_data(&msg), msg_size);

    ocrAny.release_any();
    m_msgQueue.add(msg);
}

void CZmqComm::SubThread()
{
    while (m_bRun)
    {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        // 接收消息，堵塞式
        int ret = zmq_msg_recv(&msg, m_socket, 0);
        if (ret != -1)
        {
            //处理接收数据
            ParseMsg(zmq_msg_data(&msg), zmq_msg_size(&msg));
        }
		else
		{
			break;
		}
        zmq_msg_close(&msg);
    }
    zmq_close(m_socket);
    m_socket = nullptr;
}

void CZmqComm::ParseMsg(void* data, size_t len)
{
    OCRAny oa;
    if (len && oa.ParseFromArray(data, len))
    {
        if (m_pFunc)
        {
            m_pFunc(&oa);
        }
        else
        {
            console->info("未注册处理识别结果函数");
        }
    }
    return;
}

void CZmqComm::PubThread()
{
    while (m_bRun)
    {
        zmq_msg_t msg;
        while (m_msgQueue.get(msg))
        {
            //发布数据
            zmq_msg_send(&msg, m_socket, 0);

            zmq_msg_close(&msg);
        }
        SleepMilliseconds(1);
    }
    zmq_close(m_socket);

    m_socket = nullptr;
}
void CZmqComm::XPubMsg(gui::pb::OCRAny &ocrAny)
{
	if (!m_bRun)
	{
		return;
	}

	std::shared_ptr<gui::pb::OCRAny> pCopy = std::make_shared<gui::pb::OCRAny>(ocrAny);
	m_msgOcrAny.add(pCopy);
}
void CZmqComm::XPubThread()
{
	while (m_bRun)
	{
		try
		{
			shared_ptr<OCRAny> pCopy;
			while (m_msgOcrAny.get(pCopy))
			{
				//发布数据头
				zmq_msg_t head;
				string head_key =  "svr:" + g_AppConfig.userCfg.quayNo + ":type.googleapis.com/empty_any" + "/" + pCopy->msg_id();
				if (pCopy->has_any())
				{
					head_key = "svr:" + g_AppConfig.userCfg.quayNo + ":" + pCopy->any().type_url() + "/" + pCopy->msg_id();
				}
				zmq_msg_init_size(&head, head_key.length());
				memcpy(zmq_msg_data(&head), head_key.data(), head_key.length());
				int sizeHead = zmq_msg_send(&head, m_socket, ZMQ_SNDMORE);
				zmq_msg_close(&head);
				//console->warn("发送消息头 结果：{} ", sizeHead);
				//发布数据体
				zmq_msg_t body;
				zmq_msg_init_size(&body, pCopy->ByteSizeLong());
				pCopy->SerializeToArray(zmq_msg_data(&body), pCopy->ByteSizeLong());
				int size = zmq_msg_send(&body, m_socket, 0);
				if (g_AppConfig.userCfg.sendInfoShow > 0)
				{
					if (g_AppConfig.userCfg.sendInfoShow == 1)
					{
						if (pCopy->msg_type() == MSG_TYPE::MSG_OCRDATA)
						{
							console->warn("发送消息 结果：{}  信息：{}", size, ConvertChs(pCopy->DebugString()));
						}
					}
					else if (g_AppConfig.userCfg.sendInfoShow == 2)
					{
						//需要回复的打印
						if (pCopy->need_reply() ==1 && pCopy->msg_type() != MSG_TYPE::MSG_HEART_BEAT)
						{
							console->warn("发送消息 结果：{}  信息：{}", size, ConvertChs(pCopy->DebugString()));
						}
					}
					//修改为3，留出2，用来调试，输出想看到的日志。
					else if (g_AppConfig.userCfg.sendInfoShow == 3)
					{
						if (pCopy->msg_type() == MSG_TYPE::MSG_OCRDATA
							|| pCopy->msg_type() == MSG_TYPE::MSG_EVENTINFO)
						{
							console->warn("发送消息 结果：{}  信息：{}", size, ConvertChs(pCopy->DebugString()));
						}
					}
					else if (g_AppConfig.userCfg.sendInfoShow == 4)
					{
						if (pCopy->msg_type() == MSG_TYPE::MSG_OCRDATA
							|| pCopy->msg_type() == MSG_TYPE::MSG_EVENTINFO
							|| pCopy->msg_type() == MSG_TYPE::MSG_STATEINFO)
						{
							console->warn("发送消息 结果：{}  信息：{}", size, ConvertChs(pCopy->DebugString()));
						}
					}
					else if (g_AppConfig.userCfg.sendInfoShow == 5)
					{
						if (pCopy->msg_type() == MSG_TYPE::MSG_OCRDATA
							|| pCopy->msg_type() == MSG_TYPE::MSG_EVENTINFO
							|| pCopy->msg_type() == MSG_TYPE::MSG_STATEINFO
							|| pCopy->msg_type() == MSG_TYPE::MSG_HEART_BEAT)
						{
							console->warn("发送消息 结果：{}  信息：{}", size, ConvertChs(pCopy->DebugString()));
						}
					}
					
				}
				zmq_msg_close(&body);
			}
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
		}
		catch (exception& e)
		{
			string s = e.what();
		}
		catch (...)
		{
			string s = zmq_strerror(zmq_errno());
		}
	}
	zmq_close(m_socket);
	m_socket = nullptr;
}


void CZmqComm::ReqThread()
{
    while (m_bRun)
    {
        zmq_msg_t sendMsg;
        if (m_msgQueue.get(sendMsg))
        {
            zmq_msg_send(&sendMsg, m_socket, 0);

            zmq_msg_close(&sendMsg);

            TimeMeter tm;

            tm.start();

            zmq_msg_t recvMsg;

            zmq_msg_init(&recvMsg);

            zmq_msg_recv(&recvMsg, m_socket, 0);

            //处理接收数据
            ParseMsg(zmq_msg_data(&recvMsg), zmq_msg_size(&recvMsg));

            zmq_msg_close(&recvMsg);

            console->info("问答耗时: {} ms", tm.stop());

        }
        SleepMilliseconds(1000);
    }
    zmq_close(m_socket);
    m_socket = nullptr;
}

void CZmqComm::RepThread()
{
    while (m_bRun)
    {
        zmq_msg_t recvMsg;

        zmq_msg_init(&recvMsg);

        zmq_msg_recv(&recvMsg, m_socket, 0);

        ParseMsg(zmq_msg_data(&recvMsg), zmq_msg_size(&recvMsg));

        zmq_msg_close(&recvMsg);

        zmq_msg_t sendMsg;

        if (m_msgQueue.get(sendMsg))
        {
            zmq_msg_send(&sendMsg, m_socket, 0);

            zmq_msg_close(&sendMsg);
        }
    }
    zmq_close(m_socket);

    m_socket = nullptr;
}

void CZmqComm::RecvThread()
{
    uint8_t data[256];

    while (m_bRun)
    {
        int ret = RecvData(data, sizeof(data));
        if (ret == -1)
        {
            continue;
        }
        if (ret < 0)
        {
            console->warn("[ZMQ] 接收数据时出错");
            break;
        }
        else if (ret > 0)
        {
            ParseData(data, ret);
        }
        else
        {
            continue;
        }
        SleepMilliseconds(1);
    }
}

int CZmqComm::RecvData(void *buf, size_t len)
{
    int rc = 0;
    try
    {
        rc = zmq_recv(m_socket, id_r, id_size_r, 0);
        if (rc == -1)
        {
            int en = zmq_errno();
        }

        rc = zmq_recv(m_socket, buf, len, 0);
        if (rc == -1)
        {
            int en = zmq_errno();
        }
    }
    catch (...)
    {
        console->info("错误 CZmqComm::RecvData");
    }
    return rc;
}

void CZmqComm::SendData(const void *buf, size_t len)
{
    try
    {
        int rc = zmq_send(m_socket, id, id_size, ZMQ_SNDMORE);
        if (rc == -1)
        {
            int en = zmq_errno();
            console->warn("[ZMQ] 发送数据{}", zmq_strerror(en));
            return;
        }
        rc = zmq_send(m_socket, buf, len, 0);
        if (rc == -1)
        {
            int en = zmq_errno();
            console->warn("[ZMQ] {}", zmq_strerror(en));
            return;
        }
    }
    catch (...)
    {
        console->error(" 错误：CZmqComm::SendData");
    }
    return;

}


void CZmqComm::ParseData(void* data, size_t len)
{
    try
    {
        if (m_pFuncData)
        {
            m_pFuncData(m_Name,data, len, m_pThis);
        }
        else
        {
            console->error("未注册处理识别结果函数");
        }

    }
    catch (...)
    {
        console->error(" 错误：CZmqComm::ParseData");
    }
}

void CZmqComm::ZmqMsgClose(zmq_msg_t& m)
{
    zmq_msg_close(&m);
}
