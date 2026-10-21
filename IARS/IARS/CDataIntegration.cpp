#include "CDataIntegration.h"
#include "CameraConfig.h"
#include "EnumTransform.h"
#include "TaskManager.h"
CDataIntegration::CDataIntegration(TaskManager * pMgr)
{
	m_pTaskMgr = pMgr;
}

CDataIntegration::~CDataIntegration()
{

}

struct tOriConf
{
	int recog_type;//识别类型
	float fConfidence;//置信度
	std::string ori_img_name;//原始照片名称
	std::string cam_name;//相机名称
};


//数据整合
void CDataIntegration::DataIntegration(OcrData &ocrData)
{
	ocrData.Clear();
	console->info("数据整合 workData start");
	WorkDataBody *workData = ocrData.mutable_work_data();
	m_pTaskMgr->m_mtxTaskInfo.lock();
	workData->set_work_id(m_pTaskMgr->m_JobTrigger.body().job_id());//WorkData中设置为jobid，避免相同taskid上传多次结果在GUI只显示最后一个。
	workData->set_dev_id(m_pTaskMgr->m_TaskInfo.dev_id());
	workData->set_work_lane(m_pTaskMgr->m_TaskInfo.work_lane());
	workData->set_work_type(m_pTaskMgr->m_TaskInfo.work_type());
	workData->set_spreader(m_pTaskMgr->m_TaskInfo.spreader());
	workData->set_work_status(m_pTaskMgr->m_TaskInfo.work_status());
	workData->set_snapshot_location(m_pTaskMgr->m_TaskInfo.snapshot_location());
	workData->set_stay_work(m_pTaskMgr->m_TaskInfo.stay_work());
	workData->set_job_id(m_pTaskMgr->m_JobTrigger.body().job_id());
	m_pTaskMgr->m_mtxTaskInfo.unlock();

	//WORKID不同，更新id，将g_arrTaskItem清空
	/*if (m_strWorkId != m_pTaskMgr->m_TaskInfo.work_id())
	{
		m_pTaskMgr->AddTaskLog(console->info("[CDataIntegration] 新作业开始，清空g_mapTaskItem. oldId[{}] newId[{}]", m_strWorkId, m_pTaskMgr->m_TaskInfo.work_id() ));
		m_strWorkId = m_pTaskMgr->m_TaskInfo.work_id();
		m_mapTaskItem.clear();
	}*/
	/*
	一次开闭锁之间可能存在ECS发送多次JobTrigger，每次JobTrigger发送一次结果，为了防止不同Trigger之间的识别结果有影响，将m_mapTaskItem清空。
	*/
	m_strWorkId = m_pTaskMgr->m_TaskInfo.work_id();
	m_mapTaskItem.clear();
	g_mtxPicItem.lock();
	for (auto&item:g_arrPicItem)
	{
		if (m_mapTaskItem.find(item.img_label) == m_mapTaskItem.end())
		{
			//从g_arrPicItem中选择与本次任务号相同的结果保存到g_mapTaskItem中，防止不同作业的数据混乱
			if(item.task_id == m_strWorkId)
				m_mapTaskItem[item.img_label] = item;
			else
				console->warn("[CDataIntegration] {} 任务不匹配 img_label={} task_id={} file={}", m_strWorkId, item.img_label, item.task_id, item.ori_img_name);
		}
	}
	g_mtxPicItem.unlock();
	//遍历task中的所有数据项，根据相机位置和功能筛选需要显示的图片
	//key:相机及识别类型：type_cam_ip：显示顺序按识别结果类型和相机进行排序
	map<string, tOriConf> mapOriConf;
	for (auto&item: m_mapTaskItem)
	{
		for (auto&param : item.second.arrParam)
		{
			stringstream ss;
			ss << item.second.cam_ip << "_" << param.param.type();
			string strKey = ss.str();
			//保留摄像机数据项结果置信度最高的结果
			if (mapOriConf.find(strKey) == mapOriConf.end())
			{
				m_pTaskMgr->AddTaskLog(console->info("保存ori_path：[{}] [{}]", strKey, item.second.ori_img_name));
				tOriConf ori_conf;
				ori_conf.recog_type = param.param.type();
				ori_conf.fConfidence = param.fRetConfidence;
				ori_conf.ori_img_name = item.second.ori_img_name;
				ori_conf.cam_name = item.second.cam_name;
				mapOriConf[strKey] = ori_conf;
			}
			else
			{
				if (param.fRetConfidence >= mapOriConf[strKey].fConfidence)
				{
					m_pTaskMgr->AddTaskLog(console->info("更新ori_path：[{}][{}] -> [{}][{}]",
						mapOriConf[strKey].fConfidence, mapOriConf[strKey].ori_img_name,
						param.fRetConfidence, item.second.ori_img_name));
					mapOriConf[strKey].fConfidence = param.fRetConfidence;
					mapOriConf[strKey].ori_img_name = item.second.ori_img_name;
				}
			}
		}
	}
	set<string> setOriPath;
	//发送到ECS的箱号和车号图片
	tOriConf *pTruckToEcs = nullptr;
	tOriConf *pCnToEcs = nullptr;
	for (auto&item: mapOriConf)
	{
		//将mapOriConf中的ori_path进行排重
		if (setOriPath.find(item.second.ori_img_name) == setOriPath.end())
		{
			setOriPath.insert(item.second.ori_img_name);
			auto pItem = workData->add_item_arr();
			pItem->set_img_name(item.second.ori_img_name);
			pItem->set_img_type(WorkDataBody::IMG_ORI);
		}
		//获取置信度最高的车号或箱号图片
		if (item.second.recog_type == GRpc_RecogParam::CN)
		{
			if (pCnToEcs)
			{
				if (pCnToEcs->fConfidence < item.second.fConfidence)
				{
					m_pTaskMgr->AddTaskLog(console->info("更新IMG_ECS_CN：[{}][{}] -> [{}][{}]",
						pCnToEcs->ori_img_name, pCnToEcs->fConfidence,
						item.second.ori_img_name, item.second.fConfidence));
					pCnToEcs = &item.second;
				}
			}
			else
			{
				m_pTaskMgr->AddTaskLog(console->info("保存IMG_ECS_CN：[{}][{}]",
					item.second.ori_img_name, item.second.fConfidence));
				pCnToEcs = &item.second;
			}
		}
		else if (item.second.recog_type == GRpc_RecogParam::PMN)
		{
			if (pTruckToEcs)
			{
				if (pTruckToEcs->fConfidence < item.second.fConfidence)
				{
					m_pTaskMgr->AddTaskLog(console->info("更新IMG_ECS_TRUCK：PMN[{}][{}] -> [{}][{}]",
						pTruckToEcs->ori_img_name, pTruckToEcs->fConfidence,
						item.second.ori_img_name, item.second.fConfidence));
					pTruckToEcs = &item.second;
				}
			}
			else if (item.second.fConfidence > 0)
			{
				m_pTaskMgr->AddTaskLog(console->info("保存IMG_ECS_TRUCK：PMN[{}][{}]",
					item.second.ori_img_name, item.second.fConfidence));
				pTruckToEcs = &item.second;
			}
		}
		else if (item.second.recog_type == GRpc_RecogParam::PMN_QR)
		{
			//二维码结果直接使用
			if (pTruckToEcs)
			{
				if (!item.second.cam_name.empty())
				{
					m_pTaskMgr->AddTaskLog(console->info("更新IMG_ECS_TRUCK：QR[{}][{}] -> [{}][{}]",
						pTruckToEcs->ori_img_name, pTruckToEcs->fConfidence,
						item.second.ori_img_name, item.second.fConfidence));
					//车牌相机采集的图片，cam_name为空，优先使用球机采集的车顶号
					pTruckToEcs = &item.second;
				}
			}
			else
			{
				m_pTaskMgr->AddTaskLog(console->info("保存IMG_ECS_TRUCK：QR[{}][{}]",
					item.second.ori_img_name, item.second.fConfidence));
				pTruckToEcs = &item.second;
			}
		}
	}
	if (pCnToEcs)
	{
		auto pItem = workData->add_item_arr();
		pItem->set_img_name(pCnToEcs->ori_img_name);
		pItem->set_img_type(WorkDataBody::IMG_ECS_CN);
	}
	console->info("数据整合 workData end");
	OcrDataBody *dataBody = ocrData.mutable_ocr_data();
	OcrDataBody::OcrResult *data = dataBody->mutable_original_data();
	
	bool bHasPlate = false;
	bool bHasCn = false;
	console->info("车号整合开始");
	//车头方向
	data->set_truck_head_dir(m_pTaskMgr->m_TaskInfo.truck_head_dir());
	data->set_truck_plate("T???");//车号
	data->set_truck_plate_confidence(33.0);//车号置信度

	string default_iso = "";
	if (m_pTaskMgr->m_TaskInfo.spreader() == "SINGLE40")
		default_iso = "45G1";
	else
		default_iso = "22G1";
	
	tCnItem cn_item;
	m_pTaskMgr->AddTaskLog(console->warn("[Compose] 箱号复核：---------------------------"));
	//m_pTaskMgr->ComposeCnRet(cn_item);

	if (cn_item.iso.empty() || cn_item.iso.find('?') != -1)
	{
		m_pTaskMgr->AddTaskLog(console->warn("[Compose] 使用默认ISO：{}->{}", cn_item.iso, default_iso));
		cn_item.iso = default_iso;
	}
	OcrDataBody::OcrResult::Unit *unit = data->add_unit_arr();
	//初始化初值
	unit->set_unit_number("???U???????");
	unit->set_unit_number_confidence(1.0);
	unit->set_iso_code("????");
	unit->set_iso_code_confidence(1.0);
	bool bIsEmptyTruck = true;
	if (cn_item.cn_str.length())
	{
		unit->set_unit_number(cn_item.cn_str); //箱号
		int nInvalidCh = 0;
		for (auto&ch:unit->unit_number())
		{
			if (ch == '?')
			{
				nInvalidCh++;
			}
		}
		//如果ecs没有要箱号，且箱号是???U???????,就不发送
		if (nInvalidCh >= 7)
		{
			if (m_pTaskMgr->m_JobTrigger.body().flag() == "0" || 
				m_pTaskMgr->m_JobTrigger.body().flag() == "3" || 
				m_pTaskMgr->m_JobTrigger.body().flag() == "6" ||
				m_pTaskMgr->m_JobTrigger.body().flag() == "8")
			{
				console->info("箱号结果未识别：{}，且不需要箱号结果 flag:{}，不发送", cn_item.cn_str, m_pTaskMgr->m_JobTrigger.body().flag());
				unit->set_unit_number(""); //箱号
			}
			m_pTaskMgr->AddTaskLog(console->info("[Compose] 判断为空车 箱号：{}", unit->unit_number()));
		}
		else
		{
			//箱号无效字符个数小于4个，认为有箱号结果
			bHasCn = true;
			bIsEmptyTruck = false;

			g_mtxOCRToPlc.lock();
			g_OCRToPlc.usContCount = 1;
			g_OCRToPlc.strContNo = cn_item.cn_str;
			g_mtxOCRToPlc.unlock();
		}
		unit->set_unit_number_confidence((cn_item.cnm_conf + cn_item.cnn_conf) / 2.0 *100.0);
	}
	g_mtxOCRToPlc.lock();
	if (bHasCn && m_pTaskMgr->m_TruckResultSend)
		g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_ALL;
	else if (bHasCn)
		g_OCRToPlc.resultType = OCRToPlc::RESULT_TYPE_CN;
	g_mtxOCRToPlc.unlock();
	//是否通过校验
	unit->set_unit_number_result(cn_item.bCheck ? 1 : 2);
	if (cn_item.iso.length())
	{
		unit->set_iso_code(cn_item.iso); //iso
		unit->set_iso_code_confidence(cn_item.iso_conf*100.0);

		if (bHasCn)
		{
			g_mtxOCRToPlc.lock();
			g_OCRToPlc.strContIso = cn_item.iso;
			g_mtxOCRToPlc.unlock();
		}
	}
	unit->set_position(m_pTaskMgr->m_JobTrigger.body().cont_pos());
	unit->set_door_direction("U");
	unit->set_door_direction_confidence(1.0);//置信度
	for (auto &dro : m_pTaskMgr->m_TaskInfo.ocr().dr_arr())
	{
		if (dro.dr_dir()== "FRONT")
		{
			unit->set_door_direction("F");
		}
		else if (dro.dr_dir() == "BACK")
		{
			unit->set_door_direction("B");
		}
		else
		{
			unit->set_door_direction("U");
		}
		unit->set_door_direction_confidence(dro.dr().confidence()*100.0);//置信度
	}
	if (bIsEmptyTruck)
	{
		unit->set_door_direction("U");
		m_pTaskMgr->AddTaskLog(console->info("[Compose] 判断为空车 箱门方向为U"));
	}
	m_pTaskMgr->AddTaskLog(console->info("[Compose] 箱门方向为{}", unit->door_direction()));
	g_mtxOCRToPlc.lock();
	if (unit->door_direction() == "F")
	{
		g_OCRToPlc.doorDir = OCRToPlc::DOOR_DIR_FRONT;
	}
	else if (unit->door_direction() == "B")
	{
		g_OCRToPlc.doorDir = OCRToPlc::DOOR_DIR_BACK;
	}
	else
	{
		g_OCRToPlc.doorDir = OCRToPlc::DOOR_DIR_NONE;
	}
	if (g_OCRToPlc.doorOpen == OCRToPlc::DOOR_OPEN)
	{
		unit->set_has_door_open("Y");//箱门是否打开
		unit->set_door_open_confidence(96.0);//置信度
	}
	else
	{
		unit->set_has_door_open("N");//箱门是否打开
		unit->set_door_open_confidence(96.0);//置信度
	}
	g_mtxOCRToPlc.unlock();


	//for (auto &doro : m_pTaskMgr->m_TaskInfo.ocr().dor_arr())
	//{
	//	if (doro.group() == group)
	//	{
	//		unit->set_has_door_open(EnumTransform::GetBoolDoorOpen(doro.dor().type()));//箱门是否打开
	//		unit->set_door_open_confidence(doro.dor().confidence()*100.0);//置信度
	//	}
	//}
	//for (auto &sealro : m_pTaskMgr->m_TaskInfo.ocr().sealr_arr())
	//{
	//	if (sealro.group() == group)
	//	{
	//		unit->set_has_seal(EnumTransform::GetBoolSeal(sealro.sealr().seal()));//铅封有无
	//		float confidence = 0.0;
	//		size_t size = sealro.sealr().seal_arr_size();
	//		if (size > 0)
	//		{
	//			for (auto &a_ : sealro.sealr().seal_arr())
	//			{
	//				confidence += a_.confidence();
	//			}
	//			confidence /= size;
	//		}
	//		unit->set_seal_confidence(confidence*100.0);//铅封置信度
	//		//unit->set_seal_confidence(30.45);//铅封置信度
	//	}
	//}
	//for (auto &damagero : m_pTaskMgr->m_TaskInfo.ocr().damager_arr())//残损
	//{
	//	if (damagero.group() == group)
	//	{
	//		if (damagero.damager().damage())
	//		{
	//			for (auto &a_ : damagero.damager().dmg_arr())
	//			{
	//				Damage *dmg = unit->add_damage_arr();
	//				dmg->set_type(EnumTransform::GetDamageStr(a_.type()));
	//				dmg->set_level(EnumTransform::GetDamageLevelStr(a_.level()));
	//				dmg->set_confidence(a_.box().confidence()*100.0);
	//			}
	//		}
	//	}
	//}
	//for (auto &imdgro : m_pTaskMgr->m_TaskInfo.ocr().imdgr_arr())//危标
	//{
	//	if (imdgro.group() == group)
	//	{
	//		if (imdgro.imdgr().imdg())
	//		{
	//			for (auto &a_ : imdgro.imdgr().imdg_arr())
	//			{
	//				Danger *dng = unit->add_danger_arr();
	//				dng->set_type(EnumTransform::GetImdgStr(a_.type()));
	//				dng->set_confidence(a_.box().confidence()*100.0);
	//			}
	//		}
	//	}
	//}
}

bool CDataIntegration::IsSpecialBoxIso(const string &isoStr)
{
	//特殊箱型目前只有40U6，后续有其他特殊箱型在添加
	//40U6训练时没有此数据，所以识别率不高，4个字符中 4，U, 6的识别率较高，0的识别率较低，会识别成D，字母O
	//for (const auto &specialIsoItem : g_AppConfig.vecSpecialIso)
	//{
	//	if (specialIsoItem == isoStr)
	//	{
	//		console->info("[CDataIntegration]特殊箱型。配置中特殊箱型iso：{} 当前箱iso：{}", specialIsoItem, isoStr);
	//		return true;
	//	}
	//}
	return false;
}

