#pragma once
#include "httplib.h"
#include <mutex>
#include "TaskManager.h"
#include "json.hpp"
struct OCRToSANY
{

	unsigned short usMsgNumber;		//消息编号,每次发送就自增1
	std::string taskId;			    //作业任务编号
	std::string craneId;			//设备编号
	//结果变化时记录到MainLog
	std::string  strTruckNo;		//内集卡车号
	std::string  strContNo;		    //箱号
	std::string  strContIso;		//iso
	std::string  strPlate;          //外集卡车牌号
	int doorOpen;		//箱门打开
	int doorDir;			//箱门方向
	//结果状态码，当变化时记录到MainLog
	int cn_code;
	int truck_code;
	int door_code;
	int open_code;

	enum
	{
		//DOOR_DIR
		DOOR_DIR_NONE = 0,
		DOOR_DIR_FRONT,     //前
		DOOR_DIR_BACK,   //后
		//DOOR_STATE
		DOOR_NONE = 0,
		DOOR_CLOSE,
		DOOR_OPEN,
		//RESULT_TYPE
		RESULT_TYPE_NONE = 0,
		RESULT_TYPE_PMN,     //车号
		RESULT_TYPE_CN,   //箱号
		RESULT_TYPE_ALL = 4,      //车号加箱号
	};

	OCRToSANY() : usMsgNumber(0) {}

	std::string to_string()
	{
		std::stringstream ss;
		ss << "\n";
		ss << std::setw(14) << "消息号: " << usMsgNumber << "\n";
		ss << std::setw(14) << "作业号: " << taskId << "\n";
		ss << std::setw(14) << "车号: " << strTruckNo << "\n";
		ss << std::setw(14) << "箱号: " << strContNo << "\n";
		ss << std::setw(14) << "ISO: " << strContIso << "\n";
		ss << std::setw(14) << "车牌: " << strPlate << "\n";
		return ss.str();
	}
	void Clear()
	{
		strTruckNo = "DEFAULT";		//内集卡车号
		strContNo = "DEFAULT";		    //箱号
		strContIso = "DEFAULT";		//iso
		strPlate = "DEFAULT";          //外集卡车牌号
		doorOpen = -1;		//箱门打开
		doorDir = -1;			//箱门方向
		cn_code = -1;
		truck_code = -1;
		door_code = -1;
		open_code = -1;
	}
};
class PLCHttpComm
{
public:

	PLCHttpComm(boost::shared_ptr<TaskManager> taskMgr);
	~PLCHttpComm();
public:
	bool Start();
	void Stop();
	void SendDoorDirData();
	void SendDoorOpenData();
	void SendOCRHeartBeat();
	void SendOCRSystemState();

	std::shared_ptr<httplib::Client> cliptr;
	std::shared_ptr<httplib::Server> svrptr;

	mutex m_pTaskMgrMtx;
	boost::shared_ptr<TaskManager> m_pTaskMgr;
private:
	bool m_run;

};
extern std::mutex g_OCRToSANYMtx;
extern OCRToSANY g_OCRToSANY;
