#include "IPCamera.h"
#include "Console.h"
#include "TaskManager.h"
#include "PicSave.h"
#include "PlcComm.h"
std::vector<uchar> buf_img;
IPCamera::IPCamera(std::string id, Config *config, TaskManager*pMgr)
	:CDevice(id, config)
	,m_pTaskMgr(pMgr)
{
	SetupListenerEvents();
	auto cfg = static_cast<const AxisF44Config *>(config);
	m_strIp = cfg->ip;
	m_strCamId = m_strIp + "-" + cfg->channel;
	m_strCamName = cfg->camName;
	m_cnFirst = cfg->cnFirst;
	//test BJ rtsp:///h264/ch1/main/av_stream
	//安迅士RTSP连接串
	//rtsp://user:passwd@ip:554/axis-media/media.amp?videocodec=h264&camera=1
	m_rtspUrl = "rtsp://" + cfg->userName + ":" + cfg->passwd + "@" + cfg->ip + ":554/axis-media/media.amp?videocodec=h264&camera=" + cfg->channel;
	//
	//m_rtspUrl = "rtsp://" + m_USERNAME + ":" + m_PASSWD + "@" + m_IP + ":554/h264/ch1/main/av_stream";
	
	m_exitFlag = false;
}
void IPCamera::SetupListenerEvents()
{
	auto cfg = static_cast<const AxisF44Config *>(m_devCfg);
	m_lane = cfg->lane;
	if (cfg)
	{
		for (auto& e : cfg->events.eventList)
		{
			m_EvenList[e.first] = &e.second;
		}
	}
}

void IPCamera::DealImage(tCapImage& cap_image)
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
	item_src.lane = m_lane;
	item_src.task_id = m_pTaskMgr->m_task_id;
	//****  g_picNum++ 和 item.img_label = g_picNum;之间有几个语句，会造成问题线程安全问题。添加mutex 
	g_imgLabelMutex.lock();
	g_picNum++;
	item_src.img_label = g_picNum;
	g_imgLabelMutex.unlock();
	item_src.ori_name = saveImgName;
	item_src.ori_img_name = saveImgName + "_ORI.jpg";

	shared_ptr<cv::Mat> pImg = cap_image.pMat;

	auto gunCfg = static_cast<const HKGunConfig *>(m_devCfg);
	//保存图片
	if (m_pTaskMgr->m_taskState == 1)
	{
		g_pPicSave->SavePic(pImg, imgPath + "_ORI.jpg", nullptr, nullptr, true, item_src.img_label, m_pTaskMgr->m_TaskInfo.snapshot_location() + "\\" + saveImgName + "_ORI.jpg");
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

void IPCamera::MakeRImage(RRIMAGELIST& rrImageList, tCapImage &cap_image, shared_ptr<cv::Mat> pImg, const RecogParam& rp, string saveImgName)
{
	shared_ptr<cv::Mat> pImgCvt = nullptr;
	string strConvertName;
	try
	{
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
				set_rotate_angle(*pImg, *pImgCvt, rp.rotate);
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
	}
	catch (std::exception& e)
	{
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}]处理[{}]异常{}", GetInstallNmae(), GetDeviceIP(), saveImgName, e.what()));
	}
	catch (...)
	{
		m_pTaskMgr->AddTaskLog(console->error("相机:[{} {}]处理[{}]异常", GetInstallNmae(), GetDeviceIP(), saveImgName));
	}
	auto gunCfg = static_cast<const AxisF44Config *>(m_devCfg);
	tPicItem item;

	item.cnFirstSend = m_cnFirst;
	item.pTaskMgr = m_pTaskMgr;
	item.cam_name = GetInstallNmae();
	item.cam_ip = GetDeviceIP();
	item.group = 0;
	item.lane = m_lane;
	item.lane = gunCfg->lane;
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
	pRImage->set_img_label(item.img_label);
	pRImage->set_img_type(GetInstallNmae());
	pRImage->set_cam_id(GetDeviceIP());
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

		//将Mat转换为buf
		Mat2JPEG(*pImgCvt, rrCvt);

		item.cvt_path = m_pTaskMgr->m_TaskInfo.local_snapshot_location() + cvtImgName;
		//保存图片
		if (m_pTaskMgr->m_taskState == 1)
		{
			g_pPicSave->SavePic(pImgCvt, item.cvt_path, nullptr, nullptr, false, item.img_label);
		}
	}
	else
	{
		//从吊具相机读取出来的数据即为Mat，如果未设置图像处理参数，则将Mat转换为buf
		Mat2JPEG(*pImg, *cap_image.pJPEG);
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


// 抓拍图片 码流抓图
bool IPCamera::Capture(Mat& mat)
{
	if (!isOpened())
	{
		return false;
	}
	bool ret = false;
	int nCount = 0;
	do 
	{
		m_matMutex.lock();
		ret = m_cap.read(mat);
		m_matMutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		nCount++;
	} while (!ret && nCount < 10);
	return ret;

}

void IPCamera::Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic)
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

int IPCamera::StartThreads()
{
	auto th = std::thread
	{
		[](IPCamera *cam)
		{
			console->info("{} 事件<监听>线程启动", cam->GetDeviceIP());
			PLCInfo plcInfo;
			SubscribePLC(cam->m_strCamId, plcInfo);

			while (1)
			{
				// 等待PLC更新消息

				boost::unique_lock <boost::mutex> lck(g_PlcUpdateMutex);

				g_PlcUpdateCond.wait(lck);

				lck.unlock();

				cam->m_plcInfoState.Update(plcInfo);

				const auto& events = cam->m_EvenList;
				bool bIsGetCnResult = cam->m_getCnResult;
				bool bIsGetPmnResult = cam->m_getPmnResult;
				for (auto &t : events)
				{
					if (t.second->Triggered(cam->m_plcInfoState,t.first, bIsGetCnResult, bIsGetPmnResult))
					{
						cam->Action(t.first, cam->m_EvenList.at(t.first));
					}
				}
				if (bIsGetCnResult)
				{
					cam->m_getCnResult = false;
				}
				if (bIsGetPmnResult)
				{
					cam->m_getPmnResult = false;
				}
				//bool bIsShow = PCI_IsShow();
			};
			UnSubscribePLC(cam->m_strCamId);
			console->info("{} 事件<监听>线程退出", cam->GetDeviceIP());
		}, this
	};
	th.detach();
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

	// 离线检测与自动重连
	th = std::thread{
		[&]()
		{	
			console->info("{}离线检测与自动重连线程启动",GetDeviceIP());
			m_tLastReadData = time(nullptr);
			int nFailReadCount = 0;
			int nLogFlag = 0;					// 日志打印标志位，避免日志打印过多，只在在线断线切换时打印

			while (1)
			{
				nFailReadCount++;

				bool flagOpen = true;
				bool flagGrab = true;
				bool flagNotBlackImg = true;
				double pixel_avg = 0.0;

				do
				{
					flagOpen = isOpened();
					if (!flagOpen)
					{
						break;
					}

					Mat _img;
					m_matMutex.lock();
					//flagGrab = m_cap.grab();
					flagGrab = m_cap.read(_img);
					m_matMutex.unlock();
					flagGrab = (flagGrab && !_img.empty());
					if (!flagGrab)
					{
						break;
					}

					flagNotBlackImg = ((pixel_avg = calc_image_pixel_avg(_img)) > 10);
					if (!flagNotBlackImg)
					{
						break;
					}
				} while (0);

				if (flagOpen && flagGrab && flagNotBlackImg)
				{
					m_tLastReadData = time(nullptr);
					nFailReadCount = 0;

					if (nLogFlag != 1)
					{
						console->warn("Axis 相机[{}]检测一次在线，断连标记[{}][{}][{}]，像素均值[{}]，总次数[{}]，时间戳{}，距离上次正常间隔[{}s] ",
											GetDeviceIP(), flagOpen, flagGrab, flagNotBlackImg, pixel_avg, nFailReadCount, m_tLastReadData, time(nullptr) - m_tLastReadData);
						nLogFlag = 1;
					}
				}
				else
				{
					if (nLogFlag != 2)
					{
						console->error("Axis 相机[{}]检测一次离线，断连标记[{}][{}][{}]，像素均值[{}]，总次数[{}]，时间戳{}，距离上次正常间隔[{}s] ",
							GetDeviceIP(), flagOpen, flagGrab, flagNotBlackImg, pixel_avg, nFailReadCount, m_tLastReadData, time(nullptr) - m_tLastReadData);
						nLogFlag = 2;
					}
				}

				// 读取错误的时间差大于10s，重新连接
				if (time(nullptr) - m_tLastReadData >= CAM_AXIS_DISCONN_TIME_OUT &&
					nFailReadCount > 0)
				{
					console->error("Axis 相机[{}]开始掉线自动重连, fail count[{}]，断连标记[{}][{}][{}]", GetDeviceIP(), nFailReadCount, flagOpen, flagGrab, flagNotBlackImg);
					LogOut();
					//std::this_thread::sleep_for(std::chrono::seconds(3));
					LogIn();
				}

				// 状态检测周期不能太长 - 实时从控制器把图拿走，避免控制器的图像缓存影响检测结果
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				//bool bIsShow = PCI_IsShow();
			}
		}
	};
	th.detach();
	return -1;
}

int IPCamera::Action(const std::string &eventName, const Event *event)
{
	auto pInfo = console->info("相机:[{}] 触发[{}]事件: {}", m_strCamId, event->evtType, eventName);
	m_pTaskMgr->AddTaskLog(pInfo);
	if (event->evtType == "CAP")
	{
		std::shared_ptr<Mat> pJpeg = std::make_shared<Mat>();
		if (Capture(*pJpeg))
		{
			tCapImage cap_image;
			cap_image.event = (Event *)event;
			cap_image.eventName = eventName;
			cap_image.pMat = pJpeg;
			cap_image.pJPEG = std::make_shared<JPEGPic>();
			m_queueCapImage.add(cap_image);
		}
		else
		{
			m_pTaskMgr->AddTaskLog(console->error("相机:[{}] 触发[{}]事件: {} 采集图像数据失败", m_strCamId, event->evtType, eventName));
		}
	}
	else
		m_pTaskMgr->AddTaskLog(console->error("相机:[{}] 触发[{}]事件: {} 无效", m_strCamId, event->evtType, eventName));
	return 0;
}
bool IPCamera::isOpened()
{
	std::lock_guard<std::mutex> lck(m_matMutex);
	return m_cap.isOpened();
}

// 抓拍图片 设备抓图
bool IPCamera::CaptureNEW(Mat& mat)
{
	return Capture(mat);
}

void IPCamera::StopThreads()
{
	m_exitFlag = true;

}
bool IPCamera::LogIn()
{

	if (isOpened())
	{
		UpdateStatus(LOGGEDIN);
		return true;
	}
	console->info("[{}] Open [{}]", m_strCamId, m_rtspUrl);

	m_matMutex.lock();
	bool bOpen = m_cap.open(m_rtspUrl);
	m_matMutex.unlock();
	if (bOpen)
	{
		//m_tLastReadData = time(nullptr);
		UpdateStatus(LOGGEDIN);
		console->info("[{}] Open [{}] start 时间戳{}", m_strCamId, m_rtspUrl, m_tLastReadData);
	}
	else
	{
		console->error("[{}] Open [{}] fail", m_strCamId, m_rtspUrl);
	}
	return bOpen;

}
// 设备登出
void IPCamera::LogOut()
{
	console->info("[{}] LogOut", m_strCamId);
	if (isOpened())
	{
		m_matMutex.lock();
		m_cap.release();
		m_matMutex.unlock();
		UpdateStatus(LOGGEDOUT);
		console->info("[{}] LogOut ok", m_strCamId);
		return;
	}
	console->info("[{}] LogOut closed", m_strCamId);
}
// 开始取流
bool IPCamera::StartStreaming()
{
	if (IsStreaming())
	{
		UpdateStatus(STREAMMING);
		return true;
	}
	LogIn();
	if (!isOpened())
	{
		console->error("[{}] Open [{}] fail", m_strCamId, m_rtspUrl);
		return false;
	}
	UpdateStatus(STREAMMING);
	m_matMutex.lock();
	int nImgW = (int)m_cap.get(CV_CAP_PROP_FRAME_WIDTH);
	int nImgH = (int)m_cap.get(CV_CAP_PROP_FRAME_HEIGHT);
	m_matMutex.unlock();
	console->info("[{}] Open [{}] success ({},{})", m_strCamId, m_rtspUrl, nImgW, nImgH);
	return isOpened();
}
// 停止取流
void IPCamera::StopStreaming() { return; }
// 开始解码
bool IPCamera::StartDecoding() { return isOpened(); }
// 停止解码
void IPCamera::StopDecoding() { return; }
// 设备运行状态检查
void IPCamera::RemoteControl()
{
	console->info("状态更新时间={}", time(nullptr) - m_tLastReadData);
	//20秒未更新数据，认为异常
	if (time(nullptr) - m_tLastReadData >= CAM_AXIS_DISCONN_TIME_OUT)
	{
		SetStateType(StateInfo_RUN_STATE_RUN_FAULT);
		//m_tLastReadData = time(nullptr);
		PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), " 设备在线状态监测 失败",
			StateInfo_RUN_STATE_RUN_FAULT, GetDeviceIP());

		console->warn("相机状态FAULT, IP={}", GetDeviceIP());
		console->error("{} 相机当前检测离线: 状态{}", GetDeviceIP(), stateType);
	}
	else
	{
		SetStateType(StateInfo_RUN_STATE_RUN_NORMAL);
		PackStateInfo(StateInfo_ITEM_TYPE_ITEM_CAMERA, GetInstallNmae(), " 设备在线状态监测 正常",
			StateInfo_RUN_STATE_RUN_NORMAL, GetDeviceIP());

		console->warn("相机状态NORMAL, IP={}", GetDeviceIP());
		console->warn("{} 相机当前检测在线: 状态{},间隔{}s", GetDeviceIP(), stateType, time(nullptr) - m_tLastReadData);
	}
};

// 返回 IP 地址
std::string IPCamera::GetDeviceIP()
{
	return m_strCamId;
}

// 返回 安装位置
std::string IPCamera::GetInstallNmae() { return m_strCamName; }
IPCamera::~IPCamera()
{
	/*delete[] rrImage.jpeg.buf;
	rrImage.jpeg.buf = nullptr;*/
}
// 是否已登录
bool IPCamera::HasLogIn()
{
	return isOpened();
}
// 是否在取流
bool IPCamera::IsStreaming()
{
	return isOpened();
}
// 是否在解码
bool IPCamera::IsDecoding()
{
	return isOpened();
}