#include "PicSave.h"
#include "AppConfig.h"
#include "TaskManager.h"
PicSave::PicSave()
{
	m_bRun = false;
	//创建FTP保存失败的存放路径
	m_FtpSaveErrorPath = GetAppPath() + "\\FTPReload\\";
	MakeDirs(m_FtpSaveErrorPath);
}

PicSave::~PicSave()
{

}

bool PicSave::SavePic(shared_ptr<cv::Mat> pImg, const std::string &path, std::shared_ptr<std::vector<PicText>> textArr, std::shared_ptr < std::vector<PicRec>> recArr, bool saveToFtp, int img_label, const std::string &ftpPath)
{
	//加入保存队列
	return m_SavePicQue.add(tSavePic{ saveToFtp, pImg, img_label, path , ftpPath,textArr ,recArr });
}

bool PicSave::SavePicToFtp(int img_label, const std::string &path, const std::string &FTPPath)
{
	if (!g_AppConfig.ftpCfg.enable)
	{
		return true;
	}
	string ftp_id = GetSysTimeString();
	g_mtxPicItem.lock();
	for (auto&item: g_arrPicItem)
	{
		if (img_label == item.img_label)
		{
			ftp_id = item.task_id;
			break;
		}
	}
	g_mtxPicItem.unlock();

	ftp_id += "_";
	ftp_id += to_string(img_label);
	m_SaveFtp.add(tSaveFtp{ ftp_id, path, FTPPath, 0 });
	////保存到管理队列
	//boost::mutex::scoped_lock lock(m_WaitSaveFtpMutex);
	//m_WaitSaveFtp.push_back(tSaveFtp{ uuidStr, path, FTPPath, 0 });
	//保存本地
	FtpPath ftpPath;
	ftpPath.set_img_local_path(path);
	ftpPath.set_img_ftp_path(FTPPath);
	WriteProtoToLocalFile(m_FtpSaveErrorPath + ftp_id, ftpPath);
	
	return true;
}

void PicSave::ReSaveToFtp()
{
	if (m_WaitSaveFtp.empty())
	{
		return;
	}
	boost::mutex::scoped_lock lock(m_WaitSaveFtpMutex);
	for (auto iter = m_WaitSaveFtp.begin(); iter != m_WaitSaveFtp.end(); iter++)
	{
		//大于重复保存的次数，删除
		if (iter->count > g_AppConfig.ftpCfg.reSaveCounts)
		{
			try
			{
				string src_file = m_FtpSaveErrorPath + iter->id;
				string dst_file = m_FtpSaveErrorPath + "FTPFail\\" + iter->id;
				boost::filesystem::copy_file(src_file, dst_file);
				console->info("[PicSave] 重复保存图片到FTP超限，移动文件到FTPFail，id:{} localPath:{}   FtpPath:{}", iter->id, iter->src_path, iter->ftp_path);
				m_nFtpFailCount++;
				if (m_nFtpFailCount >= 1000)
				{
					string strFail = m_FtpSaveErrorPath + "FTPFail\\";
					vector<std::string> arrFile;
					GetFilesNames(strFail, arrFile);
					int nCount = arrFile.size();
					//如果FTPFail中文件个数已经大于1000，则只保留500，其余删除，避免FTP异常时文件一直增多
					for (int nIdx = 500; nIdx < nCount; ++nIdx)
					{
						RemoveLocalFile(strFail + arrFile[nIdx]);
					}
					arrFile.clear();
					GetFilesNames(strFail, arrFile);
					m_nFtpFailCount = arrFile.size();
					console->warn("[PicSave] FTPFail文件个数{}超限，删除文件剩余{}个", nCount, m_nFtpFailCount);
				}
			}
			catch (std::exception& e)
			{
				console->error("[PicSave] 移动文件 {} 异常 {}", iter->id, e.what());
			}
			catch (...)
			{
				console->error("[PicSave] 移动文件 {} 异常", iter->id);
			}
			//删除本地数据
			RemoveLocalFile(m_FtpSaveErrorPath + iter->id);
		}
		else 
		{
			iter->count += 1;
			console->info("[PicSave] 再次保存图片到FTP count:{}，localPath:{}   FtpPath:{}", iter->count, iter->src_path, iter->ftp_path);
			m_SaveFtp.add(tSaveFtp{ iter->id, iter->src_path,iter->ftp_path, iter->count });
		}

	}
	//删除管理队列数据
	m_WaitSaveFtp.clear();
}

bool PicSave::Start()
{
	if (m_bRun)
	{
		return true;
	}
	if (g_AppConfig.ftpCfg.enable)
	{
		// FTP模块
		try
		{
			CFtpClient::getInstence().SetFtpParam(g_AppConfig.ftpCfg.ftpHost,
				g_AppConfig.ftpCfg.ftpPort,
				g_AppConfig.ftpCfg.ftpUser,
				g_AppConfig.ftpCfg.ftpPwd);
			console->info("FTP连接正常 参数为 {}/{}@{}:{}",
				g_AppConfig.ftpCfg.ftpUser,
				g_AppConfig.ftpCfg.ftpPwd,
				g_AppConfig.ftpCfg.ftpHost,
				g_AppConfig.ftpCfg.ftpPort);
		}
		catch (std::exception &e)
		{
			console->error("FTP连接失败 参数为 {}/{}@{}:{}  errorMsg:{}",
				g_AppConfig.ftpCfg.ftpUser,
				g_AppConfig.ftpCfg.ftpPwd,
				g_AppConfig.ftpCfg.ftpHost,
				g_AppConfig.ftpCfg.ftpPort,
				e.what());
			return false;
		}
		catch (...)
		{
			console->error("FTP连接失败 参数为 {}/{}@{}:{}",
				g_AppConfig.ftpCfg.ftpUser,
				g_AppConfig.ftpCfg.ftpPwd,
				g_AppConfig.ftpCfg.ftpHost,
				g_AppConfig.ftpCfg.ftpPort);
			return false;
		}
		//获取之前未保存成功的FTP信息
		GetErrorSavePath(m_FtpSaveErrorPath);

		string strFail = m_FtpSaveErrorPath + "FTPFail\\";
		MakeDirs(strFail);
		vector<std::string> arrFile;
		GetFilesNames(strFail, arrFile);
		m_nFtpFailCount = arrFile.size();
		//获取之前未保存成功的FTP信息
		GetErrorSavePath(strFail);
	}
	else
	{
		console->warn("FTP未启用");
	}
	
	m_bRun = true;
	m_thFtp = std::thread
	{
		[&]()
		{
			console->warn("PicSave Start");
			while (m_bRun)
			{
				tSavePic sl;
				if (m_SavePicQue.get(sl))
				{
					// 记录一下还有多少数据没有处理
					console->debug("[PicSave]未保存数据{}", sl.src_path, sl.ftp_path, m_SavePicQue.size());

					//保存的视频 pImg是nullptr
						//保存视频
					if (sl.src_path.find(".mp4") != string::npos)
					{
						console->debug("[PicSave]视频保存路径：src_path:{}  ftp_path:{}", sl.src_path, sl.ftp_path);
						if (sl.isSaveToFtp && g_AppConfig.ftpCfg.enable)
						{
							SavePicToFtp(sl.img_label, sl.src_path, sl.ftp_path);
						}
					}
					else //保存图片
					{
						if (sl.pImg == nullptr)
						{
							console->error("图片保存到本地 图片指针为空 路径：imgLabel:{}  path:{}", sl.img_label, sl.src_path);
						}
						else
						{
							cv::Mat matImg;
							sl.pImg->copyTo(matImg);
							if (sl.txtArr != nullptr)
							{
								for (auto text : *sl.txtArr)
								{
									cv::putText(matImg, text.text, text.org, text.fontFace, text.fontScale, text.color, text.thickness);
								}
							
							}
							if (sl.rectArr != nullptr)
							{
								for (auto rect : *sl.rectArr)
								{
									DrawRectAreas(matImg, rect.x, rect.y, rect.w, rect.h, rect.r, rect.g, rect.b);
								}
							}
							if (!cv::imwrite(sl.src_path, matImg))
							{
								console->error("图片保存到本地 错误 路径：imgLabel:{}  path:{}  w{} h{}", sl.img_label, sl.src_path, sl.pImg->cols, sl.pImg->rows);
							}
							else
							{
								console->debug("图片保存到本地 路径：imgLabel:{}  path:{} 保存到FTP：{}", sl.img_label, sl.src_path, sl.isSaveToFtp);
								if (sl.isSaveToFtp && g_AppConfig.ftpCfg.enable)
								{
									console->debug("图片保存到FTP img_label：{}  src_path:{}  dstPath:{}", sl.img_label, sl.src_path, sl.ftp_path);
									SavePicToFtp(sl.img_label, sl.src_path, sl.ftp_path);
								}
							}
						}
					}
				}
				tSaveFtp sf;
				if (m_SaveFtp.get(sf))
				{
					//保存FTP成功
					boost::mutex::scoped_lock lock(m_WaitSaveFtpMutex);
					if (SaveToFtp(sf.src_path, sf.ftp_path))
					{
						//删除本地数据
						console->info("[PicSave] 保存FTP图片成功，清理本地文件 id:{}  path:{}  ftpPath:{}, localfile:{}", sf.id, sf.src_path, sf.ftp_path, m_FtpSaveErrorPath + sf.id);
						RemoveLocalFile(m_FtpSaveErrorPath + sf.id);
					}
					//保存FTP失败
					else
					{
						console->error("[PicSave] 保存FTP图片失败，id:{}  path:{}  ftpPath:{}" , sf.id, sf.src_path, sf.ftp_path);
						//添加到管理队列
						m_WaitSaveFtp.push_back(tSaveFtp{sf.id, sf.src_path,sf.ftp_path,sf.count});
					}
				}
				ReSaveToFtp();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			console->warn("PicSave Exit");
		}
	};

	return true;
}

void PicSave::Stop()
{
	if (m_bRun)
	{
		m_bRun = false;
		m_thFtp.detach();
	}
}

void PicSave::GetErrorSavePath(const std::string &path)
{
	vector<std::string> arrName;
	GetFilesNames(path, arrName);
	for (auto &name : arrName)
	{
		//读取本地数据
		FtpPath fp;
		if (!ReadLocalFileToProto(path + name, fp))
		{
			continue;
		}
		if (-1 != GetFileAttributes(fp.img_local_path().c_str()))
		{
			//文件存在,添加到管理队列
			tSaveFtp sf;
			sf.id = name;
			sf.src_path = fp.img_local_path();
			sf.ftp_path = fp.img_ftp_path();
			sf.count = 0;
			boost::mutex::scoped_lock lock(m_WaitSaveFtpMutex);
			m_WaitSaveFtp.push_back(sf);
		}
		else
			console->error("文件读取失败 {}", fp.img_local_path());
	}
}
void PicSave::ProcessPic(const GRpc_RepeadResult &result)
{
	if (result.result_size() <= 0)
	{
		return;
	}
	for (auto&ret: result.result())
	{
		bool bFind = false;
		tPicItem pic_item;
		g_mtxPicItem.lock();
		string p, t, z;
		for (auto& item : g_arrPicItem)
		{
			if (item.img_label == ret.img_label())
			{
				item.crs_deal = true;
				bFind = true;
				pic_item = item;
				p = to_string(item.P);
				t = to_string(item.T);
				z = to_string(item.Z);
				console->warn("crs_deal taskid:{} img_label:{} path:{}", item.task_id, item.img_label, item.ori_img_name);
				break;
			}
		}
		g_mtxPicItem.unlock();

		if (!bFind)
		{
			console->error("{} 未找到对应数据项", ret.img_label());
			g_mtxPicItem.lock();
			for (auto& item : g_arrPicItem)
			{
				console->warn("cam:{} group:{} img_label:{} path:{}",
					item.cam_ip, item.group, item.img_label, item.ori_img_name);
			}
			g_mtxPicItem.unlock();
			return;
		}
		std::string singlePicPath = pic_item.ori_name;
		//做个数据拷贝，防止处理结果写入到磁盘后与原始结果相同
		cv::Mat matImg;
		if (pic_item.pImgCvt)
			pic_item.pImgCvt->copyTo(matImg);
		else
			pic_item.pImgOri->copyTo(matImg);

		//秀英相机截图，图片上不会显示PTZ，图片上自己写上PTZ
		std::shared_ptr<vector<PicText>> textParams = std::make_shared<vector<PicText>>();
		std::shared_ptr < vector<PicRec>> rectParams = std::make_shared<vector<PicRec>>();
		
		string ptzStr = "P:" + p + " T:" + t + " Z:" + z;
		/*cv::putText(matImg, ptzStr, cv::Point(10, matImg.rows - 50),
			cv::FONT_HERSHEY_SIMPLEX,
			2.0,
			cv::Scalar(255, 255, 0),
			2.0);*/

		textParams->push_back(PicText{ ptzStr, cv::Point(10, matImg.rows - 50),
			cv::FONT_HERSHEY_SIMPLEX,
			2.0,
			cv::Scalar(255, 255, 0),
			2.0 });

		int roi_x = 0;
		int roi_y = 0;
		ocrs::pb::Rect roi;
		if (ret.recogresult().Is<CnResult>())
		{
			CnResult cnr;
			if (!ret.recogresult().UnpackTo(&cnr))
				return;
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::CN, roi))
			{
				//DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				roi_x = roi.x();
				roi_y = roi.y();

				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
			}
			auto r = &cnr;
			/*DrawRectAreas(matImg, 
				roi_x + r->box().rect_cnm().rect().x(), 
				roi_y + r->box().rect_cnm().rect().y(),
				r->box().rect_cnm().rect().w(), 
				r->box().rect_cnm().rect().h());
			DrawRectAreas(matImg, 
				roi_x + r->box().rect_cnn().rect().x(), 
				roi_y + r->box().rect_cnn().rect().y(), 
				r->box().rect_cnn().rect().w(), 
				r->box().rect_cnn().rect().h());
			DrawRectAreas(matImg, 
				roi_x + r->box().rect_iso().rect().x(), 
				roi_y + r->box().rect_iso().rect().y(),
				r->box().rect_iso().rect().w(),
				r->box().rect_iso().rect().h());*/
			std::string cnm = r->cn().cnm();
			float cnm_f = r->box().rect_cnm().confidence();
			std::string cnn = r->cn().cnn();
			float cnn_f = r->box().rect_cnn().confidence();
			std::string iso = r->cn().iso();
			float iso_f = r->box().rect_iso().confidence();
			std::string info_cnm = "cnm:" + cnm + " confidence:" + to_string(cnm_f);
			/*cv::putText(matImg, info_cnm, cv::Point(10, matImg.rows - 300),
				cv::FONT_HERSHEY_SIMPLEX,
				2.0,
				cv::Scalar(255, 255, 0),
				2.0);*/

			std::string info_cnn = "cnn:" + cnn + " confidence:" + to_string(cnn_f);
			/*cv::putText(matImg, info_cnn, cv::Point(10, matImg.rows - 200),
				cv::FONT_HERSHEY_SIMPLEX,
				2.0,
				cv::Scalar(255, 255, 0),
				2.0);*/

			std::string info_iso = "iso:" + iso + " confidence:" + to_string(iso_f);
			/*cv::putText(matImg, info_iso, cv::Point(10, matImg.rows - 100),
				cv::FONT_HERSHEY_SIMPLEX,
				2.0,
				cv::Scalar(255, 255, 0),
				2.0);*/

			textParams->push_back(PicText{ info_cnm, cv::Point(10, matImg.rows - 300),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });
			textParams->push_back(PicText{ info_cnn, cv::Point(10, matImg.rows - 200),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });
			textParams->push_back(PicText{ info_iso, cv::Point(10, matImg.rows - 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });

			rectParams->push_back(PicRec{ roi_x + r->box().rect_cnm().rect().x(),roi_y + r->box().rect_cnm().rect().y(),r->box().rect_cnm().rect().w(),r->box().rect_cnm().rect().h(), 255, 255, 0 });
			rectParams->push_back(PicRec{ roi_x + r->box().rect_cnn().rect().x(),roi_y + r->box().rect_cnn().rect().y(),r->box().rect_cnn().rect().w(),r->box().rect_cnn().rect().h(), 255, 255, 0 });
			rectParams->push_back(PicRec{ roi_x + r->box().rect_iso().rect().x(),roi_y + r->box().rect_iso().rect().y(),r->box().rect_iso().rect().w(),r->box().rect_iso().rect().h(), 255, 255, 0 });

			string cn_iso = cnm + cnn + iso;
			for (auto& c : cn_iso)
			{
				if (c == '?')
					c = 'x';
			}
			string save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_CN_" + cn_iso + "_REC.jpg";
			//保存图片
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				//shared_ptr<cv::Mat> pImg = std::make_shared<cv::Mat>(matImg);
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<DoorResult>())
		{
			DoorResult dr;
			if (!ret.recogresult().UnpackTo(&dr))
			{
				return;
			}
			auto r = &dr;
			float door_f = r->confidence();
			std::string doorstr = PicSave::GetDoorStr(r->type());
			std::string info_door = "door:" + doorstr + " confidence:" + to_string(door_f);
			//cv::putText(matImg, info_door, cv::Point(10, 100),
			//	cv::FONT_HERSHEY_SIMPLEX,
			//	2.0,
			//	cv::Scalar(255, 255, 0),
			//	2.0);
			textParams->push_back(PicText{ info_door, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });
			string src_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_DOOR_" + doorstr + "_REC.jpg";
			//保存图片
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				//shared_ptr<cv::Mat> pImg = std::make_shared<cv::Mat>(matImg);
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, src_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<DoorOPENResult>())
		{
			DoorOPENResult door_open;
			if (!ret.recogresult().UnpackTo(&door_open))
			{
				return;
			}
			DoorOPENResult *r = &door_open;
			float dooro_f = r->confidence();
			std::string doorOpenstr = PicSave::GetDoorOpenStr(r->type());
			std::string info_dooro = "dooropen:" + doorOpenstr + " confidence:" + to_string(dooro_f);
			//cv::putText(matImg, info_dooro, cv::Point(10, 100),
			//	cv::FONT_HERSHEY_SIMPLEX,
			//	2.0,
			//	cv::Scalar(255, 255, 0),
			//	2.0);

			textParams->push_back(PicText{ info_dooro, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });

			//保存图片
			string save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_DOORSTATE_" + doorOpenstr + "_REC.jpg";
			//保存图片
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				if (pic_item.pImgCvt)
					pic_item.pImgCvt->copyTo(matImg);
				else
					pic_item.pImgOri->copyTo(matImg);
				//shared_ptr<cv::Mat> pImg = std::make_shared<cv::Mat>(matImg);
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt==nullptr ? pic_item.pImgOri: pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<PMResult>())
		{
			PMResult pmr;
			if (!ret.recogresult().UnpackTo(&pmr))
			{
				return;
			}
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::PMN, roi))
			{
				//DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });

				roi_x = roi.x();
				roi_y = roi.y();
			}
			PMResult *r = &pmr;
			//DrawRectAreas(matImg,
			//	roi_x + r->rect_pmn().rect().x(),
			//	roi_y + r->rect_pmn().rect().y(),
			//	r->rect_pmn().rect().w(),
			//	r->rect_pmn().rect().h(), 0);
			rectParams->push_back(PicRec{
				roi_x + r->rect_pmn().rect().x(),
				roi_y + r->rect_pmn().rect().y(),
				r->rect_pmn().rect().w(),
				r->rect_pmn().rect().h(), 0,255,0 });
			std::string pmn = r->pmn();
			//车号结果映射
			GetTruckNumFromMap(pmn);
			double pmn_f = r->rect_pmn().confidence();
			std::string info_pmn = "pmn:" + pmn + " confidence:" + to_string(pmn_f);
			//cv::putText(matImg, info_pmn, cv::Point(10, 100),
			//	cv::FONT_HERSHEY_SIMPLEX,
			//	2.0,
			//	cv::Scalar(0, 0, 255),
			//	2.0);
			textParams->push_back(PicText{ info_pmn, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(0, 0, 255),2.0 });

			//保存图片
			for (auto& c : pmn)
			{
				if (c == '?')
					c = 'x';
			}
			string save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath +"_imglabel_"+ to_string(ret.img_label())+ "_PMN_" + pmn + "_REC.jpg";
			//保存图片
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}

		}
		else if (ret.recogresult().Is<QRResult>())
		{
			QRResult qr;
			if (!ret.recogresult().UnpackTo(&qr))
			{
				return;
			}
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::PMN_QR, roi))
			{
				//DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
				roi_x = roi.x();
				roi_y = roi.y();
			}
			QRResult *r = &qr;
			//DrawRectAreas(matImg,
			//	roi_x + r->rect_qr().rect().x(),
			//	roi_y + r->rect_qr().rect().y(),
			//	r->rect_qr().rect().w(),
			//	r->rect_qr().rect().h(), 0);
			rectParams->push_back(PicRec{ roi_x + r->rect_qr().rect().x(),
				roi_y + r->rect_qr().rect().y(),
				r->rect_qr().rect().w(),
				r->rect_qr().rect().h(), 0, 255,0 });
			std::string pmn = r->qr();
			double pmn_f = r->rect_qr().confidence();
			std::string info_pmn = "qr:" + pmn + " confidence:" + to_string(pmn_f);
			/*cv::putText(matImg, info_pmn, cv::Point(10, 100),
				cv::FONT_HERSHEY_SIMPLEX,
				2.0,
				cv::Scalar(0, 0, 255),
				2.0);*/
			textParams->push_back(PicText{ info_pmn, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(0, 0, 255),2.0 });
			//保存图片
			for (auto& c : pmn)
			{
				if (c == '?')
					c = 'x';
			}
			string save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_QR_" + pmn + "_REC.jpg";
			//保存图片
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<SealResult>())
		{
			SealResult sr;
			if (!ret.recogresult().UnpackTo(&sr))
			{
				return;
			}
			if (!sr.seal())
			{
				return;
			}
			//铅封结果需要显示到GUI上，因此在原图上处理 yeming-2022-1-19
			pic_item.pImgOri->copyTo(matImg);
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::SEAL, roi))
			{
				//DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
				roi_x = roi.x();
				roi_y = roi.y();
			}
			for (auto& seal:sr.seal_arr())
			{
				//DrawRectAreas(matImg,
				//	roi_x + seal.rect().x(),
				//	roi_y + seal.rect().y(),
				//	seal.rect().w(),
				//	seal.rect().h(),
				//	0, 0, 255);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
			}
			int sealn = sr.seal_arr_size();
			std::string info_seal = "seal num:" + to_string(sealn);
			//cv::putText(matImg, info_seal, cv::Point(10, 100),
			//	cv::FONT_HERSHEY_SIMPLEX,
			//	2.0,
			//	cv::Scalar(255, 255, 0),
			//	2.0);
			textParams->push_back(PicText{ info_seal, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(255, 255, 0),2.0 });
			//保存图片
			std::string sealFtpPath;
			sealFtpPath = pic_item.pTaskMgr->m_TaskInfo.snapshot_location() + "\\seal_" + singlePicPath + "_REC.jpg";
			pic_item.pTaskMgr->m_mtxTaskInfo.lock();
			string sealPaths = pic_item.pTaskMgr->m_TaskInfo.seal_paths();
			if (!sealPaths.empty())
			{
				pic_item.pTaskMgr->m_TaskInfo.set_seal_paths(sealPaths + ";seal_" + singlePicPath + "_REC.jpg");
			}
			else
			{
				pic_item.pTaskMgr->m_TaskInfo.set_seal_paths("seal_" + singlePicPath + "_REC.jpg");
			}
			pic_item.pTaskMgr->m_mtxTaskInfo.unlock();
			auto save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_SEAL_" + to_string(sealn) + "_REC.jpg";
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<ImdgResult>())
		{
			ImdgResult imdg_result;
			if (!ret.recogresult().UnpackTo(&imdg_result))
			{
				return;
			}
			if (!imdg_result.imdg())
			{
				return;
			}
			//危标结果需要显示到GUI上，因此在原图上处理 yeming-2022-1-19
			pic_item.pImgOri->copyTo(matImg);
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::IMDG, roi))
			{
				//DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
				roi_x = roi.x();
				roi_y = roi.y();
			}
			bool bValidImdg = false;
			for (auto& imdg:imdg_result.imdg_arr())
			{
				//DrawRectAreas(matImg,
				//	roi_x + imdg.box().rect().x(),
				//	roi_y + imdg.box().rect().y(),
				//	imdg.box().rect().w(),
				//	imdg.box().rect().h(), 0, 0, 255);
				rectParams->push_back(PicRec{ roi_x + imdg.box().rect().x(),
					roi_y + imdg.box().rect().y(),
					imdg.box().rect().w(),
					imdg.box().rect().h(), 0, 0, 255 });
				std::string imdgstr = PicSave::GetImdgStr(imdg.type());
				//cv::putText(matImg, "imdg:" + imdgstr, cv::Point(imdg.box().rect().x(),
				//	imdg.box().rect().y() - 100),
				//	cv::FONT_HERSHEY_SIMPLEX,
				//	2.0,
				//	cv::Scalar(0, 0, 255),
				//	2.0);
				textParams->push_back(PicText{ imdgstr, cv::Point(imdg.box().rect().x(), imdg.box().rect().y() - 100),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(0, 0, 255),2.0 });
				if (imdg.IMDG_TYPE_IsValid(imdg.type()) &&
					imdg.type() != ImdgResult::ImdgInfo::NONE)
				{
					//如果存在不是T_NONE的结果时，才认为有效
					bValidImdg = true;
				}
			}
			int imdgn = imdg_result.imdg_arr_size();
			std::string info_imdg = "imdg num:" + to_string(imdgn);
			//cv::putText(matImg, info_imdg, cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX,
			//	5.0, cv::Scalar(0, 0, 255),
			//	5.0);
			textParams->push_back(PicText{ info_imdg, cv::Point(10, 100),cv::FONT_HERSHEY_SIMPLEX,5.0,cv::Scalar(0, 0, 255),5.0 });
			//保存图片
			std::string dangerFtpPath;
			dangerFtpPath = pic_item.pTaskMgr->m_TaskInfo.snapshot_location() + "\\danger_" + singlePicPath + "_REC.jpg";
			if (bValidImdg)
			{
				pic_item.pTaskMgr->m_mtxTaskInfo.lock();
				string dangerPaths = pic_item.pTaskMgr->m_TaskInfo.danger_paths();
				if (!dangerPaths.empty())
				{
					pic_item.pTaskMgr->m_TaskInfo.set_danger_paths(dangerPaths + ";danger_" + singlePicPath + "_REC.jpg");
				}
				else
				{
					pic_item.pTaskMgr->m_TaskInfo.set_danger_paths("danger_" + singlePicPath + "_REC.jpg");
				}
				pic_item.pTaskMgr->m_mtxTaskInfo.unlock();
			}
			else
			{
				pic_item.pTaskMgr->AddTaskLog(console->warn("无有效危标识别结果"));
			}
			auto save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_danger_" + to_string(imdgn) + "_REC.jpg";
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else if (ret.recogresult().Is<DamageResult>())
		{
			DamageResult dmg_result;
			if (!ret.recogresult().UnpackTo(&dmg_result))
			{
				return;
			}
			if (!dmg_result.damage())
			{
				return;
			}
			//残损结果需要显示到GUI上，因此在原图上处理 yeming-2022-1-19
			pic_item.pImgOri->copyTo(matImg);
			if (pic_item.GetRoiSrcByRType(GRpc_RecogParam::DAMAGE, roi))
			{
				DrawRectAreas(matImg, roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0);
				rectParams->push_back(PicRec{ roi.x(), roi.y(), roi.w(), roi.h(), 255, 0, 0 });
				roi_x = roi.x();
				roi_y = roi.y();
			}
			bool bValidDamage = false;
			for (auto&dmg: dmg_result.dmg_arr())
			{
				//DrawRectAreas(matImg,
				//	roi_x + dmg.box().rect().x(),
				//	roi_y + dmg.box().rect().y(),
				//	dmg.box().rect().w(),
				//	dmg.box().rect().h(),
				//	0, 255, 255);
				rectParams->push_back(PicRec{ roi_x + dmg.box().rect().x(),
					roi_y + dmg.box().rect().y(),
					dmg.box().rect().w(),
					dmg.box().rect().h(),
					0, 255, 255 });
				std::string damageStr = PicSave::GetDamageStr(dmg.type());
				std::string damageLevel = PicSave::GetDamageLevelStr(dmg.level());
				std::string info_damage = "damage：" + damageStr + " Level：" + damageLevel;
				//cv::putText(matImg, info_damage, cv::Point(dmg.box().rect().x(), dmg.box().rect().y() - 10), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 255), 2.0);
				textParams->push_back(PicText{ info_damage, cv::Point(dmg.box().rect().x(), dmg.box().rect().y() - 10),cv::FONT_HERSHEY_SIMPLEX,1.0,cv::Scalar(0, 255, 255),2.0 });
				if (dmg.DAMAGE_TYPE_IsValid(dmg.type()) &&
					dmg.type() != DamageResult::DamageInfo::T_NONE)
				{
					//如果存在不是T_NONE的结果时，才认为有效
					bValidDamage = true;
				}
			}
			int damagen =dmg_result.dmg_arr_size();
			std::string info_damagen = "damage num:" + to_string(damagen);
			//cv::putText(matImg, info_damagen, cv::Point(10, 40), cv::FONT_HERSHEY_SIMPLEX,
			//	3.0,
			//	cv::Scalar(0, 255, 255),
			//	3.0);
			textParams->push_back(PicText{ info_damagen, cv::Point(10, 40),cv::FONT_HERSHEY_SIMPLEX,3.0,cv::Scalar(0,255, 255),3.0 });
			//保存图片
			std::string damageFtpPath;
			damageFtpPath = pic_item.pTaskMgr->m_TaskInfo.snapshot_location() + "\\damage_" + singlePicPath + "_REC.jpg";
			if (bValidDamage)
			{
				pic_item.pTaskMgr->m_mtxTaskInfo.lock();
				string damagePaths = pic_item.pTaskMgr->m_TaskInfo.damage_paths();
				if (!damagePaths.empty())
				{
					pic_item.pTaskMgr->m_TaskInfo.set_damage_paths(damagePaths + ";damage_" + singlePicPath + "_REC.jpg");
				}
				else
				{
					pic_item.pTaskMgr->m_TaskInfo.set_damage_paths("damage_" + singlePicPath + "_REC.jpg");
				}
				pic_item.pTaskMgr->m_mtxTaskInfo.unlock();
			}
			else
			{
				pic_item.pTaskMgr->AddTaskLog(console->warn("无有效验残识别结果"));
			}

			auto save_path = pic_item.pTaskMgr->m_TaskInfo.local_snapshot_location() + singlePicPath + "_damage_" + to_string(damagen) + "_REC.jpg";
			if (pic_item.pTaskMgr->m_taskState == 1)
			{
				shared_ptr<cv::Mat> pImg = pic_item.pImgCvt == nullptr ? pic_item.pImgOri : pic_item.pImgCvt;
				SavePic(pImg, save_path, textParams, rectParams);
			}
		}
		else
		{

		}
	}
}

std::string PicSave::GetImdgStr(ImdgResult::ImdgInfo::IMDG_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("NONE");
		break;
	case 1:
		return std::string("EXPLOSIVES");
		break;
	case 2:
		return std::string("FLAMMABLE_GAS");
		break;
	case 3:
		return std::string("NON_COMBUSTIBLE");
		break;
	case 4:
		return std::string("TOXIC_GAS");
		break;
	case 5:
		return std::string("FLAMMABLE_LIQUIDS");
		break;
	case 6:
		return std::string("FLAMMABLE_SOLID");
		break;
	case 7:
		return std::string("SELF_IGNITING");
		break;
	case 8:
		return std::string("WET_FLAMMABLE");
		break;
	case 9:
		return std::string("OXIDANT");
		break;
	case 10:
		return std::string("ORGANIC_PEROXIDE");
		break;
	case 11:
		return std::string("TOXIC");
		break;
	case 12:
		return std::string("HIGHLY_TOXIC");
		break;
	case 13:
		return std::string("RADIOACTIVITY1");
		break;
	case 14:
		return std::string("RADIOACTIVITY2");
		break;
	case 15:
		return std::string("RADIOACTIVITY3");
		break;
	case 16:
		return std::string("CORROSION");
		break;
	default:
		return std::string("NO_TYPE");
		break;
	}
}
std::string PicSave::GetDamageStr(DamageResult::DamageInfo::DAMAGE_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("T_NONE");
		break;
	case 1:
		return std::string("T_DENTED");
		break;
	case 2:
		return std::string("T_BRUISED");
		break;
	case 3:
		return std::string("T_BROKEN");
		break;
	case 4:
		return std::string("T_DISTORTED");
		break;
	case 5:
		return std::string("T_BUCKLED");
		break;
	case 6:
		return std::string("T_HOLE");
		break;
	case 7:
		return std::string("T_MISSING");
		break;
	case 8:
		return std::string("T_CUT");
		break;
	case 9:
		return std::string("T_PATCHED");
		break;
	case 10:
		return std::string("T_SCRATCHED");
		break;
	case 11:
		return std::string("T_RUSTY");
		break;
	default:
		return std::string("NO_TYPE");
		break;
	}
}
std::string PicSave::GetDamageLevelStr(DamageResult::DamageInfo::DAMAGE_LEVEL level)
{
	switch (level)
	{
	case 0:
		return std::string("L_NONE");
		break;
	case 1:
		return std::string("L_SLIGHT");
		break;
	case 2:
		return std::string("L_MODERATE");
		break;
	case 3:
		return std::string("L_SERIOUS");
		break;
	default:
		return std::string("NO_TYPE");
		break;
	}
}
std::string PicSave::GetDoorStr(int type)
{
	switch (type)
	{
	case 0:
		return std::string("None");
		break;
	case 1:
		return std::string("BACK_DOOR");
		break;
	case 2:
		return std::string("FRONT_DOOR");
		break;
	case 3:
		return std::string("FLAT");
		break;
	case 4:
		return std::string("REEFER");
		break;
	case 5:
		return std::string("TANK");
		break;
	default:
		return std::string("NO_TYPE");
		break;
	}
}
std::string PicSave::GetDoorOpenStr(DoorOPENResult::OPEN_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("UNKNOWN");
		break;
	case 1:
		return std::string("CLOSE");
		break;
	case 2:
		return std::string("OPEN");
		break;
	default:
		return std::string("NO_TYPE");
		break;
	}
}

bool PicSave::SaveToFtp(std::string& path, std::string& FTPPath)
{
	if (g_AppConfig.ftpCfg.enable)
		return CFtpClient::getInstence().UploadFile(path, FTPPath);
	return false;
}

