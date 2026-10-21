#pragma once
#include "ZmqComm.h"
class ZMQManager
{
public:
	ZMQManager();
	~ZMQManager();

public:

	bool Start();
	void Stop();
	//Xpub是否启动成功
	bool GetIsPubOk();
	//Sub是否启动成功
	bool GetIsSubOk();
	//超时
	bool IsTimeOut();
protected:
	struct needReplyMsgStruct
	{
		string id;	//消息标识
		OCRAny any;	//消息体
		int count;	//重复发送的次数
		long long timeMillSeconds;//当前时间戳
	};

	bool m_bRun; //是否运行
	bool m_ServerRun; //标识服务端是否在运行，没有应答返回,则不给服务端发送除心跳以外的消息。

	CZmqComm xPubZmq;
	boost::mutex m_NeedReplyMsgQueMutex;
	std::string savePath;
	deque<needReplyMsgStruct> m_NeedReplyMsgQue;

	void DealSub();
	void DealXPub();
	void DealStateInfo();
private:
	bool isPubOk;
	bool isSubOk;

	int64_t rrTick;         // 记录读取到消息的时刻,用于判断是否超时

private:

};
