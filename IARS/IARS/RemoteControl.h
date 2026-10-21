#ifndef REMOTECONTROL_H
#define REMOTECONTROL_H

#include "ZMQManager.h"
#include "debug.h"
#include "RecordPLCData.h"
#include "PLCHttpComm.h"
/*
PLC重新连接
相机登录
*/
class CRemoteControl
{
public:
    CRemoteControl();

    ~CRemoteControl();

    // 0：启动成功 !=0：失败
    int Start();

    void Stop();
private:
	//void DealOcrResult(ocrs::pb::OCRResult& ocrr);
protected:
	bool exitFlag;
private:
    // ZMQ
    boost::shared_ptr<ZMQManager> zmqManagerCtrl;
	boost::shared_ptr<PLCHttpComm> plcCtrl;
    // 调试
    boost::shared_ptr<CDebug> debugCtrl;
	boost::shared_ptr<TaskManager> pTaskMgr;
	string m_strTaskid;//当前处理的任务号
	int m_nTruckCount;
	string m_strTruckNum1;//车号1
	string m_strTruckNum2;//车号2
	set<string> m_setCnIso;//箱号识别结果
	void DealPlcData(unsigned char* pBuf, int nLen);
};

#endif //REMOTECONTROL_H