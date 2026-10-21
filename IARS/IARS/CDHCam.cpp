#include "CDHCam.h"
#include "TaskManager.h"
#include "Utils.h"
#include "AppConfig.h"
#include <mutex>
#include "PicSave.h"


void CDHCam::SetupListenerEvents()
{
	auto gunCfg = static_cast<const DHCamConfig *>(m_devCfg);
	m_lane = gunCfg->lane;
	if (gunCfg)
	{
		for (auto& e : gunCfg->events.eventList)
		{
			m_EvenList[e.first] = &e.second;
		}
	}
}
long long CDHCam::GetSysMillSec()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void CDHCam::DealSetEvent(std::string eventName, Event *event)
{
	auto gunCfg = static_cast<const DHCamConfig *>(m_devCfg);
	auto pInfo = console->info("相机:[{} {}] 执行[SET]事件：{} PTZ:{} {} {}",
		GetInstallNmae(), GetDeviceIP(), eventName, event->ptz.P, event->ptz.T, event->ptz.Z);
	
	
	// 解码操作
	StartDecoding();

	m_nLastP = -1;
	m_nLastT = -1;
	m_nLastZ = -1;
	m_bSetFinish = false;
	//判断是否跳转到位添加一个等待的队列，防止判断是否跳转到位线程空跑
	m_queueSetPos.add(1);
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
		pInfo = console->error("相机:[{} {}] 未找到[AUTO_CAP]关联的CAP事件 {}",
			GetInstallNmae(), GetDeviceIP(), event->autoCap);
		
	}
	//名称不为空，表示需要检测是否跳转到位及进行自动拍摄
	if (!m_strAutoCapEvent.empty())
	{
		pInfo = console->warn("相机:[{} {}]待跳转完成后自动执行[{}]",
			GetInstallNmae(), GetDeviceIP(), m_strAutoCapEvent);
		
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
	std::string setPath = m_pTaskMgr-> m_TaskInfo.local_snapshot_location() + SpliceFileName(GetDeviceIP(),event->evtName) + ".txt";
	WriteStringToTxt(setPath, setPath);

	stringstream ss;
	ss << "跳转，相机[" << GetDeviceIP() << "]触发事件[" << eventName << "]";
}

void CDHCam::DealCapEvent(std::string eventName, Event *event)
{
	m_bCapFinish = false;
	auto pInfo = console->info("相机:[{} {}] 执行[CAP]事件: {}", GetInstallNmae(), GetDeviceIP(), eventName);
	
	//记录触发[CAP]事件时的变量
	m_pCapEvent = const_cast<Event *> (event);
	m_strCapEventName = eventName;
	m_llMsCap = GetSysMillSec();

	tEvent ce;
	ce.event = const_cast<Event *> (event);
	ce.eventName = eventName;
	m_queueCap.add(ce);
}

void CDHCam::DealStepEvent(std::string eventName, Event *event)
{
	m_bStepFinish = false;
	auto pInfo = console->info("相机:[{} {}] 执行[STEP]事件: {}", GetInstallNmae(), GetDeviceIP(), eventName);
	
	for (auto&evt : event->arrStepEvent)
	{
		auto itr = m_EvenList.find(evt);
		if (itr != m_EvenList.end())
		{
			tEvent et;
			et.event = (Event *)itr->second;
			et.eventName = itr->first;
			m_queueStepEvent.push(et);
			pInfo = console->info("相机:[{} {}] 添加[{}]事件", GetInstallNmae(), GetDeviceIP(), evt);
			
		}
		else
		{
			pInfo = console->error("相机:[{} {}] [{}]事件配置错误，CameraConfig中未找到", GetInstallNmae(), GetDeviceIP(), evt);
			
		}
	}
	if (m_queueStepEvent.size())
	{
		std::thread th = std::thread
		{
			[&](CDHCam *gun)
			{
				auto gunCfg = static_cast<const DHCamConfig *>(gun->m_devCfg);

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
				string strLog = console->info("相机:[{} {}] STEP执行完成", GetInstallNmae(), GetDeviceIP());
				
			},this
		};
		th.detach();
	}
	else
	{
		//没有可以执行的事件
		m_bStepFinish = true;
		pInfo = console->error("相机:[{} {}] STEP配置错误，没有可执行事件", GetInstallNmae(), GetDeviceIP());
		
	}
}

void CDHCam::DealImage(tCapImage&cap_image)
{
	m_pTaskMgr->AddTaskLog(console->info("相机:[{} {}]触发[CAP]事件 {}", GetInstallNmae(), GetDeviceIP(), cap_image.event->evtName));

	//创建保存图片文件夹
	string saveImgFolderPath = m_pTaskMgr->m_TaskInfo.local_snapshot_location();
	string saveImgName = SpliceFileName(GetDeviceIP(), cap_image.event->evtName);
	std::string imgPath = saveImgFolderPath + saveImgName;

	tPicItem item_src;
	item_src.pTaskMgr = m_pTaskMgr;
	item_src.cam_name = GetInstallNmae();
	item_src.cam_ip = GetDeviceIP();
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

	auto gunCfg = static_cast<const HKGunConfig *>(m_devCfg);
	//保存图片
	if (m_pTaskMgr->m_taskState == 1)
	{
		g_pPicSave->SavePic(pImg, imgPath + "_ORI.jpg", nullptr, nullptr ,true, item_src.img_label, m_pTaskMgr->m_TaskInfo.snapshot_location() + "\\" + saveImgName + "_ORI.jpg");
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

void CDHCam::MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName)
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
	item.cnFirstSend = false;
	item.pTaskMgr = m_pTaskMgr;
	item.cam_name = GetInstallNmae();
	item.cam_ip = GetDeviceIP();
	item.group = 0;
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



void CDHCam::DoCapture(std::string eventName, Event *event)
{
	auto pInfo = console->info("相机:[{} {}] 执行[DoCapture] {} ", GetInstallNmae(), GetDeviceIP(), eventName);
	

	std::shared_ptr<JPEGPic> pJPEG = std::make_shared<JPEGPic>();
	if (CaptureNEW(*pJPEG))
	{
		//设置相机状态为正常
		SetStateType(StateInfo_RUN_STATE_RUN_NORMAL);
		if (pJPEG->buf && pJPEG->size > 0)
		{
			DH_PTZ_LOCATION_INFO ptz;

			this->Get_DVR_Config(&ptz);
			// 将Hex转换成十进制
			int P = ptz.nPTZPan;
			int T = ptz.nPTZTilt;
			int Z = ptz.nPTZZoom;

			console->info("[{}]获取图片时的PTZ:{} {} {}", GetDeviceIP(), P, T, Z);
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
		pInfo = console->error("相机:[{} {}] 触发[CAP]事件 失败", GetInstallNmae(), GetDeviceIP());
		
	}

}

void CDHCam::CheckPtzDone()
{
	if (m_bSetFinish)
		return;
	DH_PTZ_LOCATION_INFO pos;
	if (0 != Get_DVR_Config(&pos))
		return;
	//如果当前位置与上一个位置相同，认为到位。这种方式获取PTZ的时间间隔不能太短（100ms以上），否则两次数据相同导致误认为到位。
	/*未使用PTZ设置值与目标位置相同判断，原因如下：
	  1、设置的参数可能超出相机可设置的范围；
	  2、设置的值与从相机获取的值需要进行转换（负数加3600）；
	  3、PTZ目标值与从相机中获取到的值可能存在偏差，导致不能使用==进行判断
	*/
	if (m_nLastP == pos.nPTZPan &&
		m_nLastT == pos.nPTZTilt &&
		m_nLastZ == pos.nPTZZoom)
	{
		auto pInfo = console->warn("相机:[{} {}]跳转到位 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), pos.nPTZPan, pos.nPTZTilt, pos.nPTZZoom);
		
		if (m_bAutoCap)
		{
			// 从m_EvenList事件列表中根据CAP名字查找对应的事件参数
			for (auto& evt : m_EvenList)
			{
				if (evt.second->evtType == "CAP" &&
					evt.second->evtName == m_strAutoCapEvent)
				{
					m_bAutoCap = false;
					auto pInfo = console->warn("相机:[{} {}]跳转到位自动抓拍{}",
						GetInstallNmae(), GetDeviceIP(), m_strAutoCapEvent);
					
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
		m_nLastP = pos.nPTZPan;
		m_nLastT = pos.nPTZTilt;
		m_nLastZ = pos.nPTZZoom;
		auto pInfo = console->debug("相机:[{} {}]未跳转到位 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), pos.nPTZPan, pos.nPTZTilt, pos.nPTZZoom);
		
		//没跳转到位，继续添加跳转
		m_queueSetPos.add(1);
	}
}

int CDHCam::Action(const std::string &eventName, const Event *event)
{
	auto pInfo = console->info("相机:[{} {}] 触发[{}]事件: {}", GetInstallNmae(), GetDeviceIP(), event->evtType, eventName);
	
	if (event->evtType == "SET")
	{
		if (m_bStepFinish)
		{
			DealSetEvent(eventName, (Event *)event);
			
		}

		else
		{
			pInfo = console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName);
			
		}
	}
	else if (event->evtType == "CAP")
	{
		if (m_bStepFinish)
			DealCapEvent(eventName, (Event *)event);
		else
		{
			pInfo = console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName);
			
		}
	}
	else if (event->evtType == "STEP")
	{
		if (m_bStepFinish)
		{
			DealStepEvent(eventName, (Event *)event);
		}
		else
		{
			pInfo = console->warn("相机:[{} {}] STEP未完成，[{}]事件忽略", GetInstallNmae(), GetDeviceIP(), eventName);
			
		}
	}
	else
	{
		console->error("相机:[{} {}] 未知事件 {}", GetInstallNmae(), GetDeviceIP(), eventName);
	}
	return 0;
}

int CDHCam::SetPTZ(const PTZ& pos)
{
	//获取当前PTZ值
	DH_PTZ_LOCATION_INFO ptzCur;
	int nRet = Get_DVR_Config(&ptzCur);

	if (nRet == 0)
	{
		console->warn("相机:[{} {}]跳转开始 PTZ:[{:02x} {:02x} {:02x}] -> [{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), ptzCur.nPTZPan, ptzCur.nPTZTilt, ptzCur.nPTZZoom, pos.P, pos.T, pos.Z);
	}
	else
	{
		console->warn("相机:[{} {}]跳转开始 PTZ:[{:02x} {:02x} {:02x}]",
			GetInstallNmae(), GetDeviceIP(), pos.P, pos.T, pos.Z);
	}
	return Set_DVR_Config((long)pos.P, (long)pos.T, (long)pos.Z);
}

CDHCam::CDHCam(std::string id, Config *config , TaskManager*pMgr)
	:DHIPC(id, config)
	, m_exitFlag(false)
	, m_pCapEvent(nullptr)
	, m_llMsPtz(0)
	, m_llMsCap(0)
	, m_llMsSave(0)
	, m_bSaveData(false)
	, m_singleMp4Path("")
	, m_workId("")
	, m_pTaskMgr(pMgr)
{
	m_bSetFinish = true;
	m_bCapFinish = true;
	m_bStepFinish = true;
	m_bAutoCap = false;
	SetupListenerEvents();
}

CDHCam::~CDHCam()
{

}

void CDHCam::StopThreads()
{
	m_exitFlag = true;

	m_worker.detach();

	m_cap.detach();
}


int CDHCam::StartThreads()
{
	m_exitFlag = false;
	//接收plc线程
	m_worker = std::thread
	{
		[](CDHCam *gun)
		{
			console->info("相机:[{} {}] 事件<监听>线程启动", gun->GetInstallNmae(), gun->GetDeviceIP());
			auto gunCfg = static_cast<const DHCamConfig *>(gun->m_devCfg);
			PLCInfo plcInfo;
			SubscribePLC(gunCfg->ip, plcInfo);
			gun->StartPlateDetection();
			while (!gun->m_exitFlag)
			{
				bool islog = gun->HasLogIn();
				//如果启动程序时相机就不可用，也需要启动线程，让其重连
				if (!islog)
				{
					if (!gun->OnCmdMsg(CDevice::DEVICE_LOGIN))
					{
						Sleep(10);
						continue;
					}
				}
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
						auto plog = console->info("time:{} taskid:{} eventname:{} worklane1:{} g:{} t:{} h:{}",
							GetSysTimeString(),
							gun->m_pTaskMgr->m_task_id,
							gun->m_EvenList.at(t.first)->evtName,
							plcInfo.workLine1,
							plcInfo.gantryValue, plcInfo.trollyValue, plcInfo.hoistValue);
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
				string strInfo = console->info("相机:[{} {}] 相机抓拍： {} 前置", GetInstallNmae(), GetDeviceIP(), ce.eventName);
				
				//如果控制相机进行跳转，则等待5秒后进行图像抓拍
				while (GetSysMillSec() - m_llMsPtz < g_AppConfig.userCfg.wait_ptz_sec * 1000)
				{
					if (m_bSetFinish)
					{
						//如果已经跳转到位，直接退出
						strInfo = console->info("相机:[{} {}] 相机抓拍： {} 跳转到位", GetInstallNmae(), GetDeviceIP(), ce.eventName);
						
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
				strInfo = console->info("相机:[{} {}] 相机抓拍： {} ", GetInstallNmae(), GetDeviceIP(), ce.eventName);
				
				DoCapture(ce.eventName, ce.event);
				m_bCapFinish = true;

			}
			console->info("相机:[{} {}] 事件<监听>线程退出", GetInstallNmae(), GetDeviceIP());
		}
	};

	//定时发送相机状态信息
	std::thread th = std::thread
	{
		[](CDHCam *gun)
		{
			while (!gun->m_exitFlag)
			{
				int haveSetPos = 0;
				gun->m_queueSetPos.get(haveSetPos);
				//检查是否跳转到位并自动抓拍
				gun->CheckPtzDone();
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}, this
	};
	th.detach();



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
void SaveFile(std::string path, BYTE *pBuffer, DWORD dwBufSize)
{
	FILE *pFile = nullptr;
	fopen_s(&pFile, path.c_str(), "wb");
	if (!pFile)
	{
		console->error("存储失败：path={} ", path);
		return;
	}
	fwrite(pBuffer, 1, dwBufSize, pFile);
	fclose(pFile);
	pFile = nullptr;
	console->info("存储成功：path={} ", path);
}


BOOL PlateCallBack(LLONG   lAnalyzerHandle,
	DWORD   dwAlarmType, void    *pAlarmInfo, BYTE    *pBuffer,
	DWORD   dwBufSize, LDWORD  dwUser, int     nSequence, void   *reserved)
{
	
	CDHCam *pThis = reinterpret_cast<CDHCam*>(dwUser);

	if (pThis->stateType != StateInfo_RUN_STATE_RUN_NORMAL)
	{
		pThis->stateType = StateInfo_RUN_STATE_RUN_NORMAL;
		console->warn("相机状态NORMAL, IP={}", pThis->GetDeviceIP());
	}

	std::string timeStr;
	std::string plateImgPath;
	std::string vechileImgPath;
	int conf;
	// 车号
	//pThis->m_LicPlateResult.sLicPlate = pcNumber;

	switch (dwAlarmType)
	{
	case EVENT_IVS_TRAFFICJUNCTION:
	{
		DEV_EVENT_TRAFFICJUNCTION_INFO* info = (DEV_EVENT_TRAFFICJUNCTION_INFO*)pAlarmInfo;
		pThis->m_LicPlateResult.sLicPlate = info->stTrafficCar.szPlateNumber;
		char szTemp[100] = {};
		sprintf_s(szTemp, "%d-%02d-%02d %02d:%02d:%02d",
			info->stTrafficCar.stSnapTime.dwYear, info->stTrafficCar.stSnapTime.dwMonth, info->stTrafficCar.stSnapTime.dwDay,
			info->stTrafficCar.stSnapTime.dwHour, info->stTrafficCar.stSnapTime.dwMinute, info->stTrafficCar.stSnapTime.dwSecond);
		console->warn("017 车牌={} 速度={} 车辆标识={} 时间={} 置信度={}/{} 车头号码={}",
			pThis->m_LicPlateResult.sLicPlate, info->stTrafficCar.nSpeed, info->stTrafficCar.szVehicleSign, szTemp,
			info->stuObject.nConfidence, info->stCommInfo.nPlateRecogniseConf, info->stuPlateInfo.szFrontPlateNumber);
		timeStr = szTemp;
		
		conf = info->stuObject.nConfidence;
		//保存车牌结果图片
		if (pThis->m_pTaskMgr->m_taskState == 1)
		{
			string rootPath = pThis->m_pTaskMgr->m_TaskInfo.local_snapshot_location();
			MakeDirs(rootPath);
			std::string file_path = rootPath + "\\" + GetSysTimeString() + "_" + pThis->GetDeviceIP();

			vechileImgPath = file_path + ".jpg";

			SaveFile(file_path + ".jpg", pBuffer, dwBufSize);

			if (info->stuObject.bPicEnble)
			{
				plateImgPath = file_path + "_plate.jpg";
				SaveFile(file_path + "_plate.jpg", pBuffer + info->stuObject.stPicInfo.dwOffSet, info->stuObject.stPicInfo.dwFileLenth);
			}
		}
		//// 二值化车牌抠图                                
		//if (info->stCommInfo.stuBinarizedPlateInfo.nLength > 0)
		//{
		//	SaveFile(file_path + "_Binarized.jpg", pBuffer + info->stCommInfo.stuBinarizedPlateInfo.nOffset, info->stCommInfo.stuBinarizedPlateInfo.nLength);
		//}
		//// 车身特写抠图
		//if (info->stCommInfo.stuVehicleBodyInfo.nLength > 0)
		//{
		//	SaveFile(file_path + "_Body.jpg", pBuffer + info->stCommInfo.stuVehicleBodyInfo.nOffset, info->stCommInfo.stuVehicleBodyInfo.nLength);
		//}
		break;
	}
	case EVENT_IVS_TRAFFIC_MANUALSNAP:
	{
		DEV_EVENT_TRAFFIC_MANUALSNAP_INFO *info = (DEV_EVENT_TRAFFIC_MANUALSNAP_INFO *)pAlarmInfo;
		pThis->m_LicPlateResult.sLicPlate = info->stTrafficCar.szPlateNumber;

		char szTemp[100] = {};
		sprintf_s(szTemp, "%d-%02d-%02d %02d:%02d:%02d",
			info->stTrafficCar.stSnapTime.dwYear, info->stTrafficCar.stSnapTime.dwMonth, info->stTrafficCar.stSnapTime.dwDay,
			info->stTrafficCar.stSnapTime.dwHour, info->stTrafficCar.stSnapTime.dwMinute, info->stTrafficCar.stSnapTime.dwSecond);
		console->warn("118 车牌={} 速度={} 车辆标识={} 时间={} 置信度={}/{}",
			pThis->m_LicPlateResult.sLicPlate, info->stTrafficCar.nSpeed, info->stTrafficCar.szVehicleSign, szTemp, info->stuObject.nConfidence, info->stCommInfo.nPlateRecogniseConf);
		timeStr = szTemp;

		conf = info->stuObject.nConfidence;
		//保存车牌结果图片
		if (pThis->m_pTaskMgr->m_taskState == 1)
		{
			string rootPath = pThis->m_pTaskMgr->m_TaskInfo.local_snapshot_location();
			MakeDirs(rootPath);
			std::string file_path = rootPath + "\\" + GetSysTimeString() + "_" + pThis->GetDeviceIP();

			vechileImgPath = file_path + ".jpg";

			SaveFile(file_path + ".jpg", pBuffer, dwBufSize);
			if (info->stuObject.bPicEnble)
			{
				plateImgPath = file_path + "_plate.jpg";
				//
				SaveFile(file_path + "_plate.jpg", pBuffer + info->stuObject.stPicInfo.dwOffSet, info->stuObject.stPicInfo.dwFileLenth);
			}
		}
		//// 二值化车牌抠图                                
		//if (info->stCommInfo.stuBinarizedPlateInfo.nLength > 0)
		//{
		//	SaveFile(file_path + "_Binarized.jpg", pBuffer + info->stCommInfo.stuBinarizedPlateInfo.nOffset, info->stCommInfo.stuBinarizedPlateInfo.nLength);
		//}
		//// 车身特写抠图
		//if (info->stCommInfo.stuVehicleBodyInfo.nLength > 0)
		//{
		//	SaveFile(file_path + "_Body.jpg", pBuffer + info->stCommInfo.stuVehicleBodyInfo.nOffset, info->stCommInfo.stuVehicleBodyInfo.nLength);
		//}
		break;
	}
	default:
		break;
	}
	//pThis->m_LicPlateResult.
	// 置信度,车牌阈值范围：10-28，实际置信度=100/(28-10)*(fPlateConfidence - 10)

	std::string toReplace = "粤";
	std::string replacement = "东";
	// 计算需要替换的子串的位置和长度
	size_t startPos = pThis->m_LicPlateResult.sLicPlate.find(toReplace);
	if (startPos == std::string::npos) 
	{
		pThis->m_LicPlateResult.sLicPlate.replace(0, toReplace.length(), replacement);
	}
	console->info("是否无牌车[{}],size:{}", pThis->m_LicPlateResult.sLicPlate.empty(), pThis->m_LicPlateResult.sLicPlate.size());
	if (pThis->m_LicPlateResult.sLicPlate.size()<=2)
	{
		
		pThis->m_LicPlateResult.sLicPlate = "无牌车";
		conf=80;
		console->info("设置为无牌车{}", pThis->m_LicPlateResult.sLicPlate);
	}
	pThis->m_LicPlateResult.nBelieve = conf;
	if (pThis->m_LicPlateResult.nBelieve <0)
		pThis->m_LicPlateResult.nBelieve = 0;

	//识别时间
	pThis->m_LicPlateResult.strRecTime = timeStr;

	// 车辆照片
	if (dwBufSize > pThis->m_LicPlateResult.vehPic.size)
	{
		if (pThis->m_LicPlateResult.vehPic.buf)
			delete[]pThis->m_LicPlateResult.vehPic.buf;
		pThis->m_LicPlateResult.vehPic.buf = new BYTE[dwBufSize];
	}
	if (dwBufSize > 0)
		memcpy(pThis->m_LicPlateResult.vehPic.buf, pBuffer, dwBufSize);
	pThis->m_LicPlateResult.vehPic.size = dwBufSize;

	
	cv::Mat jpeg(1, dwBufSize, CV_8UC1, (BYTE*)pBuffer);
	shared_ptr<cv::Mat> pImg = make_shared<cv::Mat>();
	cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());
	pThis->m_LicPlateResult.lane = pThis->m_lane;
	pThis->m_LicPlateResult.sCamIp = pThis->GetDeviceIP();
	pThis->m_LicPlateResult.sFileName = vechileImgPath;
	if (pThis->m_LicPlateResult.nBelieve > 0)
		pThis->m_pTaskMgr->SetPlateResult(pThis->m_LicPlateResult, pImg);

	return 1;
}


bool CDHCam::StartPlateDetection()
{
	m_plateHandle = CLIENT_RealLoadPictureEx(m_loginUserID, 0, EVENT_IVS_ALL, true, PlateCallBack, (LDWORD)this, NULL);
	console->warn("{} 开启车牌监控:{}  0=error", GetDeviceIP(), m_plateHandle);
	if (m_plateHandle == 0)
	{

		return false;
	}
	MANUAL_SNAP_PARAMETER stuPara = { 0 };
	stuPara.nChannel = 0;
	sprintf((char *)stuPara.bySequence, "abc");
	BOOL is = CLIENT_ControlDeviceEx(m_loginUserID, DH_MANUAL_SNAP, &stuPara);
	console->warn("{} 开启车牌监控:{}  0=error", GetDeviceIP(), is);
	return true;
}
//Mat转buffer
void CDHCam::Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic)
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
GRpc_Image::Img_type CDHCam::GetImgType()
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


void CDHCam::WriteEventInfoToLocal(const string &path, const string &src)
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

