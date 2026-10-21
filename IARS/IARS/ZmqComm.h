#ifndef ZMQCOMM_H
#define ZMQCOMM_H
#include "ocrs.pb.h"
#include "zmq.h"
#include "zmq_utils.h"
#include "SyncQueue.h"
#include "AppConfig.h"
#include <string>
using namespace std;


//template<class T>

typedef void(*CallFunc)(OCRAny* pMsg);

typedef void(*CallFuncData)(std::string name, void *data, size_t size, void *pThis);

class CZmqComm
{
public:
	CZmqComm();

	~CZmqComm();

	bool Start(string ip, int port, int type);

	void Stop();

	void SetRecvHandler(CallFunc cf){ m_pFunc = cf; }

    void SetRecvHandlerData(std::string name, CallFuncData cf, void *pThis){ m_Name = name, m_pFuncData = cf; m_pThis = pThis; }

	string GetErrMsg(){ return m_err_msg; }

    void SendMsg(const google::protobuf::Message& promsg);

	void XPubMsg(gui::pb::OCRAny& ocrany);

    void SendData(const void *buf, size_t len);

	void SetSubFilter(string strFilter) { m_strSubFilter = strFilter; }
private:
	void SubThread();

	void PubThread();

	void XPubThread();

    void ReqThread();

    void RepThread();

    void RecvThread();

    int RecvData(void *buf, size_t len);

    void ParseData(void* data, size_t len);

	static void ZmqMsgClose(zmq_msg_t& m);

	void ParseMsg(void* data, size_t len);

private:
	void *m_ctx;

	void *m_socket;

    uint8_t id[256];              // ZMQ id

    size_t id_size;               // ZMQ id 大小

    uint8_t id_r[256];            // ZMQ id

    size_t id_size_r;             // ZMQ id 大小

	SyncQueue<zmq_msg_t> m_msgQueue;

	SyncQueue<std::shared_ptr<OCRAny>> m_msgOcrAny;

	string m_err_msg;

	bool m_bRun;

	bool m_bCanPub;

	CallFunc m_pFunc;

    void *m_pThis;

    std::string m_Name;

    CallFuncData m_pFuncData;

	string m_strSubFilter;
};


#endif // !ZMQCOMM_H