#include "RecordPLCData.h"

#include "AppConfig.h"

CRecodData::CRecodData()
{
    ;
}

CRecodData::~CRecodData()
{
    ;
}

void CRecodData::Start()
{
    console->info("记录数据<模块启动>");

	std::thread th = std::thread
    {
        [&]()
        {
			std::shared_ptr<tPlcData> pData;
            while (1)
            {
				m_queue.get(pData);
				SavePlcData(*pData);
            };
        }
    };
	th.detach();
    return;
}

void CRecodData::AddPlcInfo(string file_path, const PLCInfo &p)
{
	std::shared_ptr<tPlcData> pData = std::make_shared<tPlcData>();
	pData->save_path = file_path+"_plc.dat";
	pData->plc = p;
	m_queue.add(pData);
}

void CRecodData::SavePlcData(tPlcData& plcdata)
{
	FILE *pFile = nullptr;
	fopen_s(&pFile, plcdata.save_path.c_str(), "a+b");
	if (pFile)
	{
		fwrite(&(plcdata.plc.plc_msg), 1, sizeof(PLCMESSAGE), pFile);
		fclose(pFile);
		pFile = nullptr;
	}
}

CRecodData* CRecodData::GetInstance()
{
	static CRecodData _inst;
	return &_inst;
}
