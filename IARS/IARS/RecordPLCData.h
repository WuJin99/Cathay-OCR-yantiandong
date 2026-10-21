#ifndef RECORDPLCDATA_H
#define RECORDPLCDATA_H

#include "Utils.h"
#include "PLCInfo.h"
#include "CameraConfig.h"
#include <fstream>
#include "ThreadQue.hpp"
class CRecodData
{
	struct tPlcData
	{
		PLCInfo plc;
		string save_path;
	};
	CRecodData();

	~CRecodData();
public:
	static CRecodData* GetInstance();
    void Start();
	void AddPlcInfo(string file_path, const PLCInfo &p);
private:
	void SavePlcData(tPlcData& plcdata);
private:
	ThreadQueue<std::shared_ptr<tPlcData>> m_queue;
};

#endif //RECORDPLCDATA_H