#include "HKGun.h"
#include "TaskManager.h"
#include "Utils.h"
#include "AppConfig.h"
#include <mutex>
#include "PicSave.h"
#include "PlcComm.h"
#include "DeviceManager.h"

void CHKGun::SetupListenerEvents()
{
    auto gunCfg = static_cast<const HKGunConfig *>(m_devCfg);
	m_lane = gunCfg->lane;
    if (gunCfg)
    {
        for (auto& e : gunCfg->events.eventList)
        {
            m_EvenList[e.first] = &e.second;
        }
    }
}
long long CHKGun::GetSysMillSec()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void CHKGun::DealSetEvent(std::string eventName, Event *event)
{
	auto gunCfg = static_cast<const HKGunConfig *>(m_devCfg);
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 执行[SET]事件：{} PTZ:{} {} {}",
		GetInstallNmae(), GetDeviceIP(), eventName, event->ptz.P, event->ptz.T, event->ptz.Z));
	//发送event事件信息
	m_pTaskMgr->SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_CAM_SET, eventName,
		m_plcInfoState.GetCurPLCInfo().gantryValue,
		m_plcInfoState.GetCurPLCInfo().hoistValue,
		m_plcInfoState.GetCurPLCInfo().trollyValue);

	SaveRealData(true);

	// 解码操作
	StartDecoding();

	m_nLastP = -1;
	m_nLastT = -1;
	m_nLastZ = -1;
	m_bSetFinish = false;
	SetPTZ(event->ptz);
	long long llPtzMs = GetSysMillSec();
	m_strAutoCapEvent = "";
	bool bFindCapEvent = false;
	for (auto& evt : m_EvenList)
	{
		if (evt.second->evtType == "CAP" &&
			evt.second->evtName == event->autoCap)
		{
			m_strAutoCapEvent = event->autoCap;
			bFindCapEvent = true;
			break;
		}
	}
	if (!event->autoCap.empty() && !bFindCapEvent)
	{
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] 未找到[AUTO_CAP]关联的CAP事件 {}",
			GetInstallNmae(), GetDeviceIP(), event->autoCap));
	}
	//名称不为空，表示需要检测是否跳转到位及进行自动拍摄
	if (!m_strAutoCapEvent.empty())
	{
		m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]待跳转完成后自动执行[{}]",
			GetInstallNmae(), GetDeviceIP(), m_strAutoCapEvent));
		m_bAutoCap = true;
		//如果设置了自动抓拍，则不更新PTZ时间戳
		m_llMsPtz = 0;
	}
	else
	{
		m_bAutoCap = false;
		//记录PTZ时间戳
		m_llMsPtz = llPtzMs;
	}

	//保存跳转信息到本地
	if (m_pTaskMgr->m_taskState == 1)
	{
		std::string setPath = m_pTaskMgr->m_TaskInfo.local_snapshot_location() + SpliceFileName(GetDeviceIP(), event->evtName) + ".txt";
		WriteStringToTxt(setPath, setPath);
	}
	/*
	stringstream ss;
	ss << "跳转，相机[" << GetDeviceIP() << "]触发事件[" << eventName << "]";
	如果在PTZ发生在CAP之后，并且时间间隔在2秒内，则认为是同一个作业的事件，
	将CAP事件加入到m_queueCap再次抓拍（延时wait_ptz_sec秒），以便解决同时满足跳转和抓拍时，在未跳转
	到位时进行了一次抓拍，到位后没再抓拍的情况 yeming-20211201

	使用AUTO_CAP方式解决上述问题，不再增加[CAP]二次抓拍事件，yeming-20220602
	*/
	/*long lOffset = llPtzMs - m_llMsCap;
	if (lOffset > 0 && lOffset < 2000 && m_pCapEvent)
	{
		console->warn("相机:[{} {}] 生成[CAP]二次抓拍：{}", GetInstallNmae(), GetDeviceIP(), m_strCapEventName);
		m_bCapFinish = false;
		tEvent ce;
		ce.event = m_pCapEvent;
		ce.eventName = m_strCapEventName;
		m_queueCap.add(ce);

		ss.str("");
		ss << "二次取流，相机[" << GetDeviceIP() << "]触发事件[" << m_strCapEventName << "]";
		AddTaskLog(ss.str());
	}*/
}

void CHKGun::DealCapEvent(std::string eventName, Event *event)
{
	m_bCapFinish = false;
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 执行[CAP]事件: {}", GetInstallNmae(), GetDeviceIP(), eventName));
	//记录触发[CAP]事件时的变量
	m_pCapEvent = const_cast<Event *> (event);
	m_strCapEventName = eventName;
	m_llMsCap = GetSysMillSec();

	tEvent ce;
	ce.event = const_cast<Event *> (event);
	ce.eventName = eventName;
	m_queueCap.add(ce);
}

void CHKGun::DealStepEvent(std::string eventName, Event *event)
{
	m_bStepFinish = false;
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 执行[STEP]事件: {}", GetInstallNmae(), GetDeviceIP(), eventName));
	for (auto&evt : event->arrStepEvent)
	{
		auto itr = m_EvenList.find(evt);
		if (itr != m_EvenList.end())
		{
			tEvent et;
			et.event = (Event *)itr->second;
			et.eventName = itr->first;
			m_queueStepEvent.push(et);
			m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 添加[{}]事件", GetInstallNmae(), GetDeviceIP(), evt));
		}
		else
		{
			m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] [{}]事件配置错误，CameraConfig中未找到", GetInstallNmae(), GetDeviceIP(), evt));
		}
	}
	if (m_queueStepEvent.size())
	{
		std::thread th = std::thread
		{
			[&]()
			{
				int nStepState = STEP_NONE;
				bool bIsAutoCapEvent = false;
				while (1)
				{
					if (STEP_NONE == nStepState)
					{
						if (m_queueStepEvent.empty())
						{
							//全部执行完成，退出循环
							break;
						}
						tEvent evt = m_queueStepEvent.front();
						m_queueStepEvent.pop();
						if (evt.event->evtType == "SET")
						{
							nStepState = STEP_SET;
							DealSetEvent(evt.eventName, evt.event);
							bIsAutoCapEvent = !m_strAutoCapEvent.empty();
						}
						else if (evt.event->evtType == "CAP")
						{
							nStepState = STEP_CAP;
							DealCapEvent(evt.eventName, evt.event);
						}
					}
					if (STEP_SET == nStepState)
					{
						//SET事件执行完成后，将状态恢复为STEP_NONE，以便执行m_queueStepEvent中下一个
						if (m_bSetFinish)
						{
							if (bIsAutoCapEvent)
							{
								//如果SET事件设置了AUTO_CAP，等CAP完成后将状态恢复为STEP_NONE
								if (m_bCapFinish)
									nStepState = STEP_NONE;
							}
							else
							{
								//未设置AUTO_CAP，直接将状态恢复为STEP_NONE
								nStepState = STEP_NONE;
							}
						}
					}
					else if (STEP_CAP == nStepState)
					{
						//CAP事件执行完成后，将状态恢复为STEP_NONE，以便执行m_queueStepEvent中下一个
						if (m_bCapFinish)
							nStepState = STEP_NONE;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				m_bStepFinish = true;
				m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] STEP执行完成", GetInstallNmae(), GetDeviceIP()));
			}
		};
		th.detach();
	}
	else
	{
		//没有可以执行的事件
		m_bStepFinish = true;
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] STEP配置错误，没有可执行事件", GetInstallNmae(), GetDeviceIP()));
	}
}

void CHKGun::DealTriggerEvent(std::string eventName, Event *event)
{
	m_bStepFinish = false;
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 执行[TRIGGER]事件: {}", GetInstallNmae(), GetDeviceIP(), eventName));
	for (auto&evt : event->arrStepEvent)
	{
		auto itr = m_EvenList.find(evt);
		if (itr != m_EvenList.end())
		{
			tEvent et;
			et.event = (Event *)itr->second;
			et.eventName = itr->first;
			m_queueStepEvent.push(et);
			m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 添加[{}]事件", GetInstallNmae(), GetDeviceIP(), evt));
		}
		else
		{
			m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] [{}]事件配置错误，CameraConfig中未找到", GetInstallNmae(), GetDeviceIP(), evt));
		}
	}
	if (m_queueStepEvent.size())
	{
		std::thread th = std::thread
		{
			[&]()
			{
				int nStepState = STEP_NONE;
				bool bIsAutoCapEvent = false;
				while (1)
				{
					if (STEP_NONE == nStepState)
					{
						if (m_queueStepEvent.empty())
						{
							//全部执行完成，退出循环
							break;
						}
						tEvent evt = m_queueStepEvent.front();
						m_queueStepEvent.pop();
						if (evt.event->evtType == "SET")
						{
							nStepState = STEP_SET;
							DealSetEvent(evt.eventName, evt.event);
							bIsAutoCapEvent = !m_strAutoCapEvent.empty();
						}
						else if (evt.event->evtType == "CAP")
						{
							nStepState = STEP_CAP;
							DealCapEvent(evt.eventName, evt.event);
						}
					}
					if (STEP_SET == nStepState)
					{
						//SET事件执行完成后，将状态恢复为STEP_NONE，以便执行m_queueStepEvent中下一个
						if (m_bSetFinish)
						{
							if (bIsAutoCapEvent)
							{
								//如果SET事件设置了AUTO_CAP，等CAP完成后将状态恢复为STEP_NONE
								if (m_bCapFinish)
									nStepState = STEP_NONE;
							}
							else
							{
								//未设置AUTO_CAP，直接将状态恢复为STEP_NONE
								nStepState = STEP_NONE;
							}
						}
					}
					else if (STEP_CAP == nStepState)
					{
						//CAP事件执行完成后，将状态恢复为STEP_NONE，以便执行m_queueStepEvent中下一个
						if (m_bCapFinish)
							nStepState = STEP_NONE;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				m_bStepFinish = true;
				m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] TRIGGER执行完成", GetInstallNmae(), GetDeviceIP()));
			}
		};
		th.detach();
	}
	else
	{
		//没有可以执行的事件
		m_bStepFinish = true;
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] TRIGGER配置错误，没有可执行事件", GetInstallNmae(), GetDeviceIP()));
	}
}


void CHKGun::DealImage(tCapImage&cap_image)
{
	auto gunCfg = static_cast<const HKGunConfig *>(m_devCfg);
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]触发[CAP]事件 {}", GetInstallNmae(), GetDeviceIP(), cap_image.event->evtName));

	//创建保存图片文件夹
	string saveImgFolderPath = m_pTaskMgr->m_TaskInfo.local_snapshot_location();
	string saveImgName = SpliceFileName(GetDeviceIP(), cap_image.event->evtName);
	std::string imgPath = saveImgFolderPath + saveImgName;

	tPicItem item_src;
	item_src.pTaskMgr = m_pTaskMgr;
	item_src.cam_name = GetInstallNmae();
	item_src.cam_ip = GetDeviceIP();
	item_src.lane = gunCfg->lane;
	item_src.group = 0;
	item_src.task_id = m_pTaskMgr->m_task_id;
	//****  g_picNum++ 和 item.img_label = g_picNum;之间有几个语句，会造成问题线程安全问题。添加mutex
	g_imgLabelMutex.lock();
	g_picNum++;
	item_src.img_label = g_picNum;
	g_imgLabelMutex.unlock();
	item_src.ori_name = saveImgName;
	item_src.ori_img_name = saveImgName + "_ORI.jpg";

	// 解码JPEG图片
	cv::Mat jpeg(1, cap_image.pJPEG->size, CV_8UC1, cap_image.pJPEG->buf);
	shared_ptr<cv::Mat> pImg = make_shared<cv::Mat>();
	cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());

	//保存图片
	if (m_pTaskMgr->m_taskState == 1)
	{
		g_pPicSave->SavePic(pImg, imgPath + "_ORI.jpg", nullptr, nullptr, true, item_src.img_label, m_pTaskMgr->m_TaskInfo.snapshot_location() + "\\" + saveImgName + "_ORI.jpg");
		m_pTaskMgr->AddTaskLog(console->info("相机[{}][{}]保存图片 img_label:[{}] path:[{}]", GetDeviceIP(), GetInstallNmae(), item_src.img_label, item_src.ori_img_name));
	}

	if (cap_image.event->arrRecogParam.size())
	{
		RRIMAGELIST rrImageList;
		for (auto& t : cap_image.event->arrRecogParam)
		{
			MakeRImage(rrImageList, cap_image, pImg, t, saveImgName);
		}
		m_pTaskMgr->m_RImageList.add(rrImageList);
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]无识别属性", GetInstallNmae(), GetDeviceIP()));
	}
}

void CHKGun::MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName)
{
	shared_ptr<cv::Mat> pImgCvt = nullptr;
	string strConvertName;
	if (rp.rectify.length())
	{
		console->warn("相机:[{} {}]设置rectify[{}]", GetInstallNmae(), GetDeviceIP(), rp.rectify);
		MyRect rectifyRect = rp.rectify.c_str();
		if (rectifyRect.r.x >= 0 &&
			rectifyRect.r.y >= 0 &&
			rectifyRect.r.width <= pImg->cols &&
			rectifyRect.r.height <= pImg->rows)
		{
			pImgCvt = make_shared<cv::Mat>();
			cv::Point2f pos1[4] = {
				cv::Point2f(rectifyRect.r.x, rectifyRect.r.y),
				cv::Point2f(rectifyRect.r.x + rectifyRect.r.width, rectifyRect.r.y),
				cv::Point2f(rectifyRect.r.x, rectifyRect.r.y + rectifyRect.r.height),
				cv::Point2f(rectifyRect.r.x + rectifyRect.r.width, rectifyRect.r.y + rectifyRect.r.height) };
			cv::Point2f pos2[4] = { cv::Point2f(0, 0), cv::Point2f(pImg->cols, 0),
				cv::Point2f(0, pImg->rows), cv::Point2f(pImg->cols, pImg->rows) };
			cv::Mat re = getPerspectiveTransform(pos1, pos2);
			cv::warpPerspective(*pImg, *pImgCvt, re, pImg->size(), cv::INTER_CUBIC);
			strConvertName += "_rectify";
			console->warn("相机:[{} {}]设置rectify成功", GetInstallNmae(), GetDeviceIP());
		}
		else
		{
			console->error("相机:[{} {}]设置rectify[{}]无效，超出[0,0,{},{}]范围",
				GetInstallNmae(), GetDeviceIP(), rp.rectify, pImg->cols, pImg->rows);
		}
	}
	if (rp.reverse)
	{
		m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]设置图像反转", GetInstallNmae(), GetDeviceIP()));
		if (!pImgCvt)
			pImgCvt = make_shared<cv::Mat>(*pImg);
		UnevenLightCompensate(*pImgCvt);
		strConvertName += "_reverse";
	}

	if (rp.rotate)
	{
		try
		{
			m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]设置图像旋转：{}", GetInstallNmae(), GetDeviceIP(), rp.rotate));
			if (!pImgCvt)
				pImgCvt = make_shared<cv::Mat>(*pImg);
			cv::Mat cvt_;
			pImgCvt->copyTo(cvt_);
			set_rotate_angle(cvt_, *pImgCvt, rp.rotate);
			strConvertName += "_rotate_" + to_string(rp.rotate);
		}
		catch (std::exception& e)
		{
			m_pTaskMgr->AddTaskLog(console->error("图像旋转异常：{}", e.what()));
		}
		catch (...)
		{
			m_pTaskMgr->AddTaskLog(console->error("图像旋转异常"));
		}
	}

	tPicItem item;
	item.cnFirstSend = GetCnFirst();
	item.pTaskMgr = m_pTaskMgr;
	item.cam_name = GetInstallNmae();
	item.cam_ip = GetDeviceIP();
	item.group = 0;
	item.lane = m_lane;
	item.task_id = m_pTaskMgr->m_task_id;
	//****  g_picNum++ 和 item.img_label = g_picNum;之间有几个语句，会造成问题线程安全问题。添加mutex
	g_imgLabelMutex.lock();
	g_picNum++;
	item.img_label = g_picNum;
	g_imgLabelMutex.unlock();
	item.ori_name = saveImgName;
	item.ori_img_name = saveImgName + "_ORI.jpg";
	item.pImgOri = pImg;

	std::shared_ptr<ocrs::pb::GRpc_RImage> pRImage = std::make_shared<ocrs::pb::GRpc_RImage>();
	pRImage->mutable_image()->set_width(pImg->cols);
	pRImage->mutable_image()->set_height(pImg->rows);
	pRImage->set_img_label(item.img_label);
	pRImage->set_img_type(GetInstallNmae());
	pRImage->set_cam_id(GetDeviceIP());
	pRImage->mutable_image()->set_type(GetImgType());
	pRImage->mutable_image()->set_width(pImg->cols);
	pRImage->mutable_image()->set_height(pImg->rows);
	pRImage->mutable_image()->set_channel(pImg->channels());
	JPEGPic rrCvt;
	if (pImgCvt)
	{
		string cvtImgName = saveImgName + strConvertName + "_CVT.jpg";
		//进行旋转后，图像尺寸可能会改变，再次赋值
		pRImage->mutable_image()->set_width(pImgCvt->cols);
		pRImage->mutable_image()->set_height(pImgCvt->rows);
		item.pImgCvt = pImgCvt;

		Mat2JPEG(*pImgCvt, rrCvt);

		item.cvt_path = m_pTaskMgr->m_TaskInfo.local_snapshot_location() + cvtImgName;
		//保存图片
		if (m_pTaskMgr->m_taskState == 1)
		{
			g_pPicSave->SavePic(pImgCvt, item.cvt_path);
		}
	}
	else
	{
		//此处不需要转换，从HK相机读取出来的数据为buf，已经填充到pJPEG
		//Mat2JPEG(*pImg, *cap_image.pJPEG);
	}

	GRpc_RecogParam * param = pRImage->add_param();
	param->set_type(rp.rtype);
	param->mutable_roi()->set_x(rp.roi.x);
	param->mutable_roi()->set_y(rp.roi.y);
	param->mutable_roi()->set_w(rp.roi.width);
	param->mutable_roi()->set_h(rp.roi.height);

	tPicItem::tPicParam pic_param;
	pic_param.fRetConfidence = 0;
	pic_param.param = *param;
	pic_param.truckType = rp.truckType;
	item.arrParam.push_back(pic_param);

	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]发布识别照片label={}\n{}",
		GetInstallNmae(), GetDeviceIP(), item.img_label, ConvertChs(pRImage->DebugString())), true);

	g_mtxPicItem.lock();
	g_arrPicItem.push_back(item);
	g_mtxPicItem.unlock();
	if (rrCvt.buf)
	{
		pRImage->mutable_image()->set_data(rrCvt.buf, rrCvt.size);
		delete rrCvt.buf;
		rrCvt.buf = nullptr;
	}
	else
	{
		pRImage->mutable_image()->set_data(cap_image.pJPEG->buf, cap_image.pJPEG->size);
	}
	rrImageList.push_back(pRImage);
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]发布识别照片label={} proto-size:{}",
		GetInstallNmae(), GetDeviceIP(), item.img_label, pRImage->ByteSizeLong()));
}


void CHKGun::DoCapture(std::string eventName, Event *event)
{
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 执行[DoCapture] {} ", GetInstallNmae(), GetDeviceIP(), eventName));
	//发送event事件信息
	m_pTaskMgr->SendTaskInfo(EventInfo_EVENT_TYPE_EVENT_CAM_CAP, eventName,
		m_plcInfoState.GetCurPLCInfo().gantryValue,
		m_plcInfoState.GetCurPLCInfo().hoistValue,
		m_plcInfoState.GetCurPLCInfo().trollyValue);

	std::shared_ptr<JPEGPic> pJPEG = std::make_shared<JPEGPic>();
	if (CaptureNEW(*pJPEG))
	{
		//设置相机状态为正常
		SetStateType(StateInfo_RUN_STATE_RUN_NORMAL);
		if (pJPEG->buf && pJPEG->size > 0)
		{
			NET_DVR_PTZPOS ptz;

			this->Get_DVR_Config(ptz);
			// 将Hex转换成十进制
			short P = HexToDec(ptz.wPanPos);
			short T = HexToDec(ptz.wTiltPos);
			short Z = HexToDec(ptz.wZoomPos);
			tCapImage cap_image;
			cap_image.event = event;
			cap_image.eventName = eventName;
			cap_image.pJPEG = pJPEG;

			cap_image.P = P;
			cap_image.T = T;
			cap_image.Z = Z;

			m_queueCapImage.add(cap_image);
		}
	}
	else
	{
		//设置相机状态为异常
		SetStateType(StateInfo_RUN_STATE_RUN_FAULT);
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] 触发[CAP]事件 失败", GetInstallNmae(), GetDeviceIP()));
	}
}

void CHKGun::CheckPtzDone()
{
	if (m_bSetFinish)
		return;
	NET_DVR_PTZPOS pos;
	if (0 != Get_DVR_Config(pos))
		return;
	//如果当前位置与上一个位置相同，认为到位。这种方式获取PTZ的时间间隔不能太短（100ms以上），否则两次数据相同导致误认为到位。
	/*未使用PTZ设置值与目标位置相同判断，原因如下：
	  1、设置的参数可能超出相机可设置的范围；
	  2、设置的值与从相机获取的值需要进行转换（负数加3600）；
	  3、PTZ目标值与从相机中获取到的值可能存在偏差，导致不能使用==进行判断
	*/
	if (m_nLastP == pos.wPanPos &&
		m_nLastT == pos.wTiltPos &&
		m_nLastZ == pos.wZoomPos)
	{
		m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]跳转到位 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), pos.wPanPos, pos.wTiltPos, pos.wZoomPos));
		if (m_bAutoCap)
		{
			// 从m_EvenList事件列表中根据CAP名字查找对应的事件参数
			for (auto& evt : m_EvenList)
			{
				if (evt.second->evtType == "CAP" &&
					evt.second->evtName == m_strAutoCapEvent)
				{
					m_bAutoCap = false;
					m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]跳转到位自动抓拍{}",
						GetInstallNmae(), GetDeviceIP(), m_strAutoCapEvent));
					m_bCapFinish = false;
					tEvent ce;
					ce.event = const_cast<Event *>(evt.second);
					ce.eventName = evt.second->evtName;
					m_queueCap.add(ce);
					//此处不直接调用DoCapture，防止满足其他抓拍事件时，2个线程同时调用引发异常。
					//DoCapture(ce);
					break;
				}
			}
		}
		m_bSetFinish = true;
	}
	else
	{
		m_nLastP = pos.wPanPos;
		m_nLastT = pos.wTiltPos;
		m_nLastZ = pos.wZoomPos;
		m_pTaskMgr->AddTaskLog(console->debug("相机:[{} {}]未跳转到位 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), pos.wPanPos, pos.wTiltPos, pos.wZoomPos));
	}
}

int CHKGun::Action(const std::string &eventName, const Event *event)
{
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 触发[{}]事件: {}", GetInstallNmae(), GetDeviceIP(), event->evtType, eventName));
    if (event->evtType == "SET")
    {
		if (m_bStepFinish)
			DealSetEvent(eventName, (Event *)event);
		else
		{
			m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName));
		}
    } else if (event->evtType == "CAP")
	{
		if (m_bStepFinish)
			DealCapEvent(eventName, (Event *)event);
		else
		{
			m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName));
		}
	} else if (event->evtType == "STEP")
	{
		if (m_bStepFinish)
		{
			DealStepEvent(eventName, (Event *)event);
		}
		else
		{
			m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName));
		}
	} else if (event->evtType == "TRIGGER")
	{
		if (m_bStepFinish)
		{
			DealTriggerEvent(eventName, (Event *)event);
		}
		else
		{
			m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}] TRIGGER未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName));
		}
	} else {
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}] 未知事件 {}", GetInstallNmae(), GetDeviceIP(), eventName));
	}
    return 0;
}

int CHKGun::SetPTZ(const PTZ& pos)
{
	//获取当前PTZ值
	NET_DVR_PTZPOS ptzCur;
	int nRet = Get_DVR_Config(ptzCur);
    NET_DVR_PTZPOS ptz;
    PTZ _pos = pos;
    if (_pos.P < 0)
    {
        _pos.P = 3600 + _pos.P;
    }
    if (_pos.T < 0)
    {
        _pos.T = 3600 + _pos.T;
    }
    ptz.wAction = 1;
    ptz.wPanPos = DecToHex(_pos.P);
    ptz.wTiltPos = DecToHex(_pos.T);
    ptz.wZoomPos = DecToHex(_pos.Z);

	if (nRet == 0)
	{
		m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]跳转开始 PTZ:[{:02x} {:02x} {:02x}] -> [{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), ptzCur.wPanPos, ptzCur.wTiltPos, ptzCur.wZoomPos, ptz.wPanPos, ptz.wTiltPos, ptz.wZoomPos));
	}
	else
	{
		m_pTaskMgr->AddTaskLog(console->warn("相机:[{} {}]跳转开始 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), ptz.wPanPos, ptz.wTiltPos, ptz.wZoomPos));
	}
    return Set_DVR_Config(ptz);
}

CHKGun::CHKGun(std::string id, Config *config, TaskManager*pMgr)
    :CHKIPC(id, config)
    , m_exitFlag(false)
	, m_pCapEvent(nullptr)
	, m_llMsPtz(0)
	, m_llMsCap(0)
	, m_llMsSave(0)
	, m_bSaveData(false)
	, m_singleMp4Path("")
	, m_pTaskMgr(pMgr)
{
	m_bSetFinish = true;
	m_bCapFinish = true;
	m_bStepFinish = true;
	m_bAutoCap = false;
    SetupListenerEvents();
}

CHKGun::~CHKGun()
{

}

void CHKGun::StopThreads()
{
	m_exitFlag = true;

	m_worker.detach();

	m_cap.detach();
}

void CHKGun::SaveRealData(bool bOpen)
{
	//保存视频有问题，先注释
	/*
	string singleMp4Path;
	if (bOpen)
	{
		if (!m_bSaveData)
		{
			m_bSaveData = true;
			m_llMsSave = GetSysMillSec();
			m_singleMp4Path = SpliceFileName("video") + ".mp4";
			m_save_path = g_TaskInfo.local_snapshot_location() + m_singleMp4Path;


			NET_DVR_SaveRealData(m_loginUserID, (char*)m_save_path.c_str());
			console->info("[{}]开始保存视频 {}", GetDeviceIP(), m_save_path);
		}
		else
		{
			console->info("[{}]已开始保存视频 {}", GetDeviceIP());
		}
	}
	else
	{
		if (m_bSaveData)
		{
			m_bSaveData = false;
			NET_DVR_StopSaveRealData(m_loginUserID);
			//保存视频到FTP
			m_queueVideoCodec.add(m_singleMp4Path);
			//PicSave::GetInstance()->SavePicToFtp(1, m_save_path,g_TaskInfo.snapshot_location() + singleMp4Path);
			console->info("[{}]停止保存视频 {}，并上传：{}", GetDeviceIP(), m_save_path, g_TaskInfo.snapshot_location() + m_singleMp4Path);
		}
		else
		{
			console->warn("[{}]已停止保存视频", GetDeviceIP());
		}
	}
	*/
}

int CHKGun::StartThreads()
{
	m_exitFlag = false;
	//接收plc线程
	m_worker = std::thread
    {
        [](CHKGun *gun)
        {
            console->info("相机:[{} {}] 事件<监听>线程启动", gun->GetInstallNmae(), gun->GetDeviceIP());
            auto gunCfg = static_cast<const HKGunConfig *>(gun->m_devCfg);
            PLCInfo plcInfo;
            SubscribePLC(gunCfg->ip, plcInfo);

            while (1)
            {
                // 等待PLC更新消息

                boost::unique_lock <boost::mutex> lck(g_PlcUpdateMutex);

                g_PlcUpdateCond.wait(lck);

                lck.unlock();

                gun->m_plcInfoState.Update(plcInfo);
                const auto& events = gun->m_EvenList;

				bool bIsGetCnResult = gun->m_getCnResult;
				bool bIsGetPmnResult = gun->m_getPmnResult;
                for (auto &t : events)
                {
                    if (t.second->Triggered(gun->m_plcInfoState,t.first, bIsGetCnResult, bIsGetPmnResult))
                    {
                        gun->Action(t.first, gun->m_EvenList.at(t.first));

                    }
                }
				if (bIsGetCnResult)
				{
					gun->m_getCnResult = false;
				}
				if (bIsGetPmnResult)
				{
					gun->m_getPmnResult = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				//bool bIsShow = PCI_IsShow();

            };
            UnSubscribePLC(gunCfg->ip);
            console->info("相机:[{} {}] 事件<监听>线程退出",gun->GetInstallNmae(), gun->GetDeviceIP());
        }, this
    };

	//拍照线程，防止cap时set还没完成
	m_cap = std::thread
	{
		[&]()
		{
			console->info("相机:[{} {}] 事件<监听>线程启动", GetInstallNmae(), GetDeviceIP());
			while (!m_exitFlag)
			{
				tEvent ce;
				m_queueCap.get(ce);
				m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 相机抓拍： {} 前置", GetInstallNmae(), GetDeviceIP(), ce.eventName));
				//如果控制相机进行跳转，则等待5秒后进行图像抓拍
				while (GetSysMillSec() - m_llMsPtz < g_AppConfig.userCfg.wait_ptz_sec * 1000)
				{
					if (m_bSetFinish)
					{
						//如果已经跳转到位，直接退出
						m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 相机抓拍： {} 跳转到位", GetInstallNmae(), GetDeviceIP(), ce.eventName));
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
				m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}] 相机抓拍： {} ", GetInstallNmae(), GetDeviceIP(), ce.eventName));
				DoCapture(ce.eventName, ce.event);
				m_bCapFinish = true;
				/*if (m_bSaveData &&
					(GetSysMillSec() - m_llMsSave >= g_AppConfig.userCfg.wait_ptz_sec * 5 * 1000))
				{
					SaveRealData(false);
				}*/
			}
			console->info("相机:[{} {}] 事件<监听>线程退出", GetInstallNmae(), GetDeviceIP());
		}
	};

	//定时发送相机状态信息
	std::thread th = std::thread
	{
		[](CHKGun *gun)
		{
			int nStateCount = g_AppConfig.userCfg.stateInfoInterval * 1000 / 200;
			int nWaitCount = 0;
			while (!gun->m_exitFlag)
			{
				if (g_AppConfig.userCfg.stateInfoInterval > 0)
				{
					if (nWaitCount >= nStateCount)
					{
						PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, gun->GetInstallNmae(),
							"相机状态检测", gun->stateType, gun->GetDeviceIP());
						if (gun->stateType == StateInfo::RUN_STATE::StateInfo_RUN_STATE_RUN_FAULT)
						{
							console->error("相机:[{} {}]状态检测异常", gun->GetInstallNmae(), gun->GetDeviceIP());
						}
						nWaitCount = 0;
					}
					nWaitCount++;
				}
				//检查是否跳转到位并自动抓拍
				gun->CheckPtzDone();
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}, this
	};
	th.detach();


	//保存的视频重新编解码保存
	std::thread thVideoCodec = std::thread
	{
		[&](CHKGun *gun)
		{
			while (!gun->m_exitFlag)
			{
				{
					string videoSingleSavePath;
					gun->m_queueVideoCodec.get(videoSingleSavePath);
					size_t pos = string::npos;
					pos = videoSingleSavePath.find(".mp4");
					if (pos != string::npos)
					{
						string reCodeSinglePath = videoSingleSavePath.substr(0, pos) + "_ReCode.mp4";
						string reCodeFullPath = gun->m_pTaskMgr->m_TaskInfo.local_snapshot_location() + reCodeSinglePath;
						string videoFullPath = gun->m_pTaskMgr->m_TaskInfo.local_snapshot_location() + videoSingleSavePath;
						string commandStr = "ffmpeg.exe -i " + videoFullPath + " -c copy -an -v quiet -y " + reCodeFullPath;
						console->info("[videoCode]转码ffmpeg命令:{}", commandStr);
						system(commandStr.c_str());
						string ftpVideoFullPath = gun->m_pTaskMgr->m_TaskInfo.snapshot_location() + "//" + reCodeSinglePath;
						g_pPicSave->SavePic(nullptr, reCodeFullPath, nullptr, nullptr, true, -1, ftpVideoFullPath);
						console->info("[videoCode] videoSingleSavePath:{} \n reCodeSinglePath:{}\n reCodeFullPath:{}\n videoFullPath:{}\n ftpVideoFullPath:{}",
							videoSingleSavePath, reCodeSinglePath, reCodeFullPath, videoFullPath, ftpVideoFullPath);
						//上锁，上传mp4文件
						/*g_mtxTaskInfo.lock();
						string oriPaths = g_TaskInfo.ori_paths();
						if (!oriPaths.empty())
						{
							g_TaskInfo.set_ori_paths(oriPaths + ";" + reCodeSinglePath);
						}
						else
						{
							g_TaskInfo.set_ori_paths(reCodeSinglePath);
						}
						g_mtxTaskInfo.unlock();*/
					}
					else
					{
						console->error("[videoCode]视频路径中未发现.mp4后缀，不保存 path:{}", videoSingleSavePath);
					}
				}
			}

		}, this
	};
	thVideoCodec.detach();

	//图片数据处理线程，从DoCapture中分离，避免处理耗时影响相机事件延时结束
	th = std::thread
	{
		[&]()
		{
			tCapImage cap_image;
			while (1)
			{
				//get内部有锁，m_queueCapImage没数据时自行阻塞
				m_queueCapImage.get(cap_image);
				DealImage(cap_image);
				delete cap_image.pJPEG->buf;
				cap_image.pJPEG->buf = nullptr;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	};
	th.detach();

    return -1;
}
//void CHKGun::Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic)
//{
//	buf_gMutex.lock();
//	// 编码JPEG图片
//	buf_g.clear();
//	cv::imencode(".jpg", srcMat, buf_g);
//
//	if (capPic.buf == nullptr)
//	{
//		capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
//	}
//	memset(capPic.buf, 0, MAX_JPEG_BUFSIZE);
//	memcpy(capPic.buf, &buf_g[0], buf_g.size());
//	capPic.size = buf_g.size();
//	buf_gMutex.unlock();
//}

//Mat转buffer
void CHKGun::Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic)
{
	// 编码JPEG图片
	std::vector<uchar> buf_g;
	//c++14问题，vector特别大时，需要先给个capacity
	buf_g.reserve(MAX_JPEG_BUFSIZE);
	cv::imencode(".jpg", srcMat, buf_g);

	if (capPic.buf == nullptr)
	{
		capPic.buf = new BYTE[MAX_JPEG_BUFSIZE];
	}
	memset(capPic.buf, 0, MAX_JPEG_BUFSIZE);
	memcpy(capPic.buf, &buf_g[0], buf_g.size());
	capPic.size = buf_g.size();
}
GRpc_Image::Img_type CHKGun::GetImgType()
{
	if (GetInstallNmae().find("zl") != string::npos || GetInstallNmae().find("yl") != string::npos)
	{
		return GRpc_Image::BACK_DOOR; //后箱门
	}
	else if (GetInstallNmae().find("lc") != string::npos || GetInstallNmae().find("hc") != string::npos)
	{
		return GRpc_Image::SIDE_DOOR; //侧箱门
	}
	else //吊具上相机拍的顶面算侧箱门，待确认
	{
		return GRpc_Image::SIDE_DOOR;
	}
}

void CHKGun::WriteEventInfoToLocal(const string &path, const string &src)
{
	string str_pb = path;
	FILE *pOut = nullptr;
	errno_t er = fopen_s(&pOut, str_pb.c_str(), "at+");
	if (pOut)
	{
		//fseek(pOut, 0, SEEK_END);
		int nByteSize = src.length();
		//再写数据
		fwrite(src.c_str(), 1, nByteSize, pOut);
		fwrite("\n", 1, 1, pOut);
		fclose(pOut);
		pOut = nullptr;

	}
	else
	{
		console->error("lane写入txt文件失败 {} err={}", str_pb, er);
	}
}

//---------------------------------------------------------
CHKPlate::CHKPlate(std::string id, Config *config, TaskManager*pMgr)
	:CHKIPC(id, config)
	, m_exitFlag(false)
	, m_pTaskMgr(pMgr)
{
	m_pGunCfg = (HKPlateConfig *)config;
	m_lane = m_pGunCfg->lane;
	console->info("车牌 name:{} ip:{} lane:{}", m_pGunCfg->camName, m_pGunCfg->ip, m_lane);
}

CHKPlate::~CHKPlate()
{

}

void CHKPlate::StopThreads()
{
	m_exitFlag = true;

	if (!NET_DVR_CloseAlarmChan_V30(m_lHandle))
	{
	    console->error("相机:[{} {}] NET_DVR_CloseAlarmChan_V30 error, {}",
            GetInstallNmae(), GetDeviceIP(), static_cast<int>(NET_DVR_GetLastError()));
	}
}

int CHKPlate::StartThreads()
{
	m_exitFlag = false;
	//定时发送相机状态信息
	std::thread th = std::thread
	{
		[](CHKPlate *gun)
		{

			//设置报警回调函数
			BOOL ret = NET_DVR_SetDVRMessageCallBack_V50(0, MessageCallback, gun);
			console->info("相机:[{} {}] NET_DVR_SetDVRMessageCallBack_V50 {}", gun->GetInstallNmae(), gun->GetDeviceIP(), ret);

			//启用布防
			NET_DVR_SETUPALARM_PARAM struSetupParam = { 0 };
			struSetupParam.dwSize = sizeof(NET_DVR_SETUPALARM_PARAM);

			//上传报警信息类型: 0- 老报警信息(NET_DVR_PLATE_RESULT), 1- 新报警信息(NET_ITS_PLATE_RESULT)
			struSetupParam.byAlarmInfoType = 1;
			//布防等级:二级布防，针对智能交通设备
			struSetupParam.byLevel = 1;

			gun->m_lHandle = NET_DVR_SetupAlarmChan_V41(gun->m_loginUserID, &struSetupParam);
			if (gun->m_lHandle < 0)
				console->error("相机:[{} {}] 布防失败 error {}", gun->GetInstallNmae(), gun->GetDeviceIP(), NET_DVR_GetLastError());
			else
				console->info("相机:[{} {}] 布防成功", gun->GetInstallNmae(), gun->GetDeviceIP());

			int nStateCount = g_AppConfig.userCfg.stateInfoInterval * 1000 / 200;
			int nWaitCount = 0;
			while (!gun->m_exitFlag)
			{
				if (g_AppConfig.userCfg.stateInfoInterval > 0)
				{
					if (nWaitCount >= nStateCount)
					{
						PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, gun->GetInstallNmae(),
							"相机状态检测", gun->stateType, gun->GetDeviceIP());
						if (gun->stateType == StateInfo::RUN_STATE::StateInfo_RUN_STATE_RUN_FAULT)
						{
							console->error("相机:[{} {}]状态检测异常", gun->GetInstallNmae(), gun->GetDeviceIP());
						}
						nWaitCount = 0;

						if (gun->m_lHandle < 0)
						{
							gun->m_lHandle = NET_DVR_SetupAlarmChan_V41(gun->m_loginUserID, &struSetupParam);
							if (gun->m_lHandle < 0)
								console->error("相机:[{} {}] 布防失败 error {}", gun->GetInstallNmae(), gun->GetDeviceIP(), NET_DVR_GetLastError());
							else
								console->info("相机:[{} {}] 布防成功", gun->GetInstallNmae(), gun->GetDeviceIP());
						}
					}
					nWaitCount++;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}, this
	};
	th.detach();

	return -1;
}

void CHKPlate::MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser)
{
	//传入的pUser为最后一个设置回调函数的对象，需要通过IP查找实际的CHKPlate
	CHKPlate *gun = (CHKPlate*)pUser;
	auto iter = gun->m_pDevMgr->plateDevList.find(pAlarmer->sDeviceIP);
	if (iter == gun->m_pDevMgr->plateDevList.end())
		return;
	CHKPlate *pThis = iter->second;
	console->warn("车牌识别 相机:[{}] Command:[0x{:04X}] BufLen:{}", pAlarmer->sDeviceIP, lCommand, dwBufLen);
	switch (lCommand)
	{
	case COMM_ITS_PLATE_RESULT:
	{
		NET_ITS_PLATE_RESULT struITSPlateResult = { 0 };
		memcpy(&struITSPlateResult, pAlarmInfo, sizeof(struITSPlateResult));

		string strBelieve;
		int nIdx = 0;
		int nCount = 0;
		int nTotal = 0;
		for (auto& c: struITSPlateResult.struPlateInfo.sLicense)
		{
			if (c)
			{
				strBelieve += fmt::format("{},", (int)struITSPlateResult.struPlateInfo.byBelieve[nIdx]);
				//单个字符置信度不为0，取平均值作为结果置信度
				if (struITSPlateResult.struPlateInfo.byBelieve[nIdx])
				{
					nCount++;
					nTotal += struITSPlateResult.struPlateInfo.byBelieve[nIdx];
				}
			}
			nIdx++;
		}
		int nConf = 0;
		if (nCount)
			nConf = nTotal / nCount;
		string strTypes;
		for (int i = 0; i < struITSPlateResult.dwPicNum; i++)
		{
			strTypes += pThis->GetType((int)struITSPlateResult.struPicInfo[i].byType);
			strTypes += " ";
		}
		if (struITSPlateResult.dwPicNum > 0)
		{
			console->warn("车牌识别 相机:[{} {}] 车牌:[{}][{}]{} 结果[{}]",
				pThis->GetInstallNmae(), pThis->GetDeviceIP(),
				struITSPlateResult.struPlateInfo.sLicense, nConf, strBelieve, strTypes);
		}
		else
		{
			console->warn("车牌识别 相机:[{} {}] 车牌:[{}][{}]{}",
				pThis->GetInstallNmae(), pThis->GetDeviceIP(),
				struITSPlateResult.struPlateInfo.sLicense, nConf, strBelieve);
		}

		//sLicense中车牌结果包含颜色，如“黄鲁A12345”
		string strLicense = struITSPlateResult.struPlateInfo.sLicense;
		std::string plateNumber = "";
		if (strLicense.length() > 3)
			plateNumber = strLicense.substr(2, strLicense.length() - 2);
		else
			plateNumber = strLicense;

		//场景图
		BYTE *pVehicleBuf = nullptr;
		DWORD dwVehicleSize = 0;
		//车牌图
		BYTE *pPlateBuf = nullptr;
		DWORD dwPlateSize = 0;
		for (int i = 0; i < struITSPlateResult.dwPicNum; i++)
		{
			//byType取值：0-车牌图，1-场景图，2-合成图，3-特写图，4-二值图，5-码流，6-人脸子图(主驾驶)，7-人脸子图(副驾驶)，8-非机动车，9-行人，10-称重原始裸数据，11-目标图，12-主驾驶室图，13-副驾驶室图，14-人脸图抠小图
			//保存场景图
			if (struITSPlateResult.struPicInfo[i].dwDataLen != 0
				&& (struITSPlateResult.struPicInfo[i].byType == 1 || struITSPlateResult.struPicInfo[i].byType == 2)
				&& struITSPlateResult.struPicInfo[i].pBuffer)
			{
				pVehicleBuf = struITSPlateResult.struPicInfo[i].pBuffer;
				dwVehicleSize = struITSPlateResult.struPicInfo[i].dwDataLen;
			}
			//车牌图
			if (struITSPlateResult.struPicInfo[i].dwDataLen != 0
				&& struITSPlateResult.struPicInfo[i].byType == 0
				&& struITSPlateResult.struPicInfo[i].pBuffer)
			{
				pPlateBuf = struITSPlateResult.struPicInfo[i].pBuffer;
				dwPlateSize = struITSPlateResult.struPicInfo[i].dwDataLen;
			}
		}
		//添加图片：仅任务中保存图片，无任务不写图（与 ICE 侧行为一致）
		string rootPath = gun->m_pTaskMgr->m_TaskInfo.local_snapshot_location();
		std::string file_path;
		if (!rootPath.empty() && gun->m_pTaskMgr->m_taskState == 1)
		{
			MakeDirs(rootPath);
			file_path = rootPath + GetSysTimeString() + "_" + pThis->GetDeviceIP() + "_" + plateNumber;
			if (pVehicleBuf)
			{
				WriteImage(file_path + "_vehicle.jpg", pVehicleBuf, dwVehicleSize);
			}
			if (pPlateBuf)
			{
				WriteImage(file_path + "_plate.jpg", pPlateBuf, dwPlateSize);
			}
		}

		pThis->m_LicPlateResult.sLicPlate = plateNumber;
		std::string toReplace = "粤";
		std::string replacement = "东";
		// 计算需要替换的子串的位置和长度
		size_t startPos = pThis->m_LicPlateResult.sLicPlate.find(toReplace);
		if (startPos == std::string::npos)
		{
			pThis->m_LicPlateResult.sLicPlate.replace(0, toReplace.length(), replacement);
		}
		console->info("相机:[{} {}] 是否无牌车[{}],size:{}",
			pThis->GetInstallNmae(), pThis->GetDeviceIP(), pThis->m_LicPlateResult.sLicPlate.empty(), pThis->m_LicPlateResult.sLicPlate.size());
		if (pThis->m_LicPlateResult.sLicPlate.size() <= 2)
		{
			pThis->m_LicPlateResult.sLicPlate = "无牌车";
			nConf = 80;
			console->info("相机:[{} {}] 设置为无牌车{}", pThis->GetInstallNmae(), pThis->GetDeviceIP(), pThis->m_LicPlateResult.sLicPlate);
		}
		pThis->m_LicPlateResult.nBelieve = nConf;
		if (pThis->m_LicPlateResult.nBelieve < 0)
			pThis->m_LicPlateResult.nBelieve = 0;

		//识别时间
		SYSTEMTIME sysTime;
		GetLocalTime(&sysTime);
		char szTemp[100] = {};
		sprintf_s(szTemp, "%d-%02d-%02d %02d:%02d:%02d",
			sysTime.wYear, sysTime.wMonth, sysTime.wDay,
			sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
		pThis->m_LicPlateResult.strRecTime = szTemp;

		// 车辆照片
		if (dwVehicleSize > pThis->m_LicPlateResult.vehPic.size)
		{
			if (pThis->m_LicPlateResult.vehPic.buf)
				delete[]pThis->m_LicPlateResult.vehPic.buf;
			pThis->m_LicPlateResult.vehPic.buf = new BYTE[dwVehicleSize];
		}
		if (dwVehicleSize > 0)
			memcpy(pThis->m_LicPlateResult.vehPic.buf, pVehicleBuf, dwVehicleSize);
		pThis->m_LicPlateResult.vehPic.size = dwVehicleSize;


		cv::Mat jpeg(1, dwVehicleSize, CV_8UC1, (BYTE*)pVehicleBuf);
		shared_ptr<cv::Mat> pImg = make_shared<cv::Mat>();
		cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());
		pThis->m_LicPlateResult.lane = pThis->m_lane;
		pThis->m_LicPlateResult.sCamIp = pThis->GetDeviceIP();
		pThis->m_LicPlateResult.sFileName = file_path.empty() ? "" : file_path + "_vehicle.jpg";
		if (pThis->m_LicPlateResult.nBelieve > 0)
			pThis->m_pTaskMgr->SetPlateResult(pThis->m_LicPlateResult, pImg);

		break;
	}
	default:
		break;
	}
}

string CHKPlate::GetType(int type)
{
	static string arrType[] =
	{
		"0-车牌图",
		"1-场景图",
		"2-合成图",
		"3-特写图",
		"4-二值图",
		"5-码流",
		"6-人脸子图(主驾驶)",
		"7-人脸子图(副驾驶)",
		"8-非机动车",
		"9-行人",
		"10-称重原始裸数据",
		"11-目标图",
		"12-主驾驶室图",
		"13-副驾驶室图",
		"14-人脸图抠小图"
	};
	if (type >= 0 && type <= 14)
	{
		return arrType[type];
	}
	return fmt::format("{}-未知", type);
}
