#pragma once
#include "Console.h"
#include "ocrs.pb.h"
#include "Utils.h"
class CDataIntegration
{
public:
	CDataIntegration(TaskManager * pMgr);
	~CDataIntegration();
public:
	//数据整合
	void DataIntegration(OcrData &ocrData);
	//判断ISO是否是特殊箱型ISO
	bool IsSpecialBoxIso(const string &isoStr);
private:
	TaskManager *m_pTaskMgr;
	string m_strWorkId;
	//key：img_label
	map<int, tPicItem> m_mapTaskItem;
};

