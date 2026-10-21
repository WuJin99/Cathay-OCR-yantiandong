#ifndef DEBUG_H
#define DEBUG_H

#include "TimeMeter.hpp"
#include "tokenizer.h"
#include "Console.h"
#include "TaskManager.h"
#include "DeviceManager.h"
#include "Utils.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2\highgui\highgui.hpp>
#include "FtpClient.h"
#include "PlcComm.h"

class CDebug
{
public:
	struct tImage
	{
		string file_name;
		string cam_pos;
		string cam_ip;
		vector<RecogParam> arrRecogParam;
	};
    CDebug()
    {
    }
    ~CDebug()
    {
        exitFlag = true;
        worker.detach();
    }

    std::vector<string> m_imgList;
    void Start()
    {
        console->info("调试 <<模块启动>>");

        exitFlag = false;

        worker = std::thread
        {
            [&]()
            {
                while (!exitFlag)
                {

                    std::string cmd;
                    std::getline(std::cin, cmd);
                    std::vector<std::string> args;
                    tokenize(cmd, args);
					if (cmd.empty())
					{
						console->error("invalid input command,input help for details");
						continue;
					}
                    console->warn("{}", cmd);
					if (args.size() <= 0)
					{
						continue;
					}

                    if (_stricmp(args[0].c_str(), "read") == 0)
                    {
                        if (args.size() != 3)
                        {
                            console->info("输入命令错误：read + dir/jpg + path");
							continue;
                        }
                        if (_stricmp(args[1].c_str(), "jpg") == 0)
                        {
							JPEGPic rrImage;
							bool bRet = ReadImage(args[2].c_str(), rrImage.buf, rrImage.size);
							if (bRet)
							{
								// 解码JPEG图片
								m_imgList.push_back(args[2]);
								console->info("读取 {} 成功，图像size {}", args[2], rrImage.size);
							}
							else
								console->error("读取 {} 失败", args[2]);
							if (rrImage.buf)
							{
								delete[] rrImage.buf;
								rrImage.buf = nullptr;
							}
							console->info("图像队列中现存图像{}个", m_imgList.size());
                        }
                        else if (_stricmp(args[1].c_str(), "dir") == 0)
                        {
                            std::vector<std::string> file_list;
                            get_file_names(args[2], "jpg", file_list);
							for (auto &t : file_list)
							{
								JPEGPic rrImage;
								ReadImage(t.c_str(), rrImage.buf, rrImage.size);

								if (rrImage.buf)
								{
									m_imgList.push_back(t);
									console->info("读取 {} 成功，图像size {}", t, rrImage.size);
									delete rrImage.buf;
									rrImage.buf = nullptr;
								}
								else
									console->error("读取 {} 失败", t);
							}
							console->info("图像队列中现存图像{}个", m_imgList.size());
                        }
                    }
                    else if (_stricmp(args[0].c_str(), "cap") == 0)
                    {
						if (args.size() != 2)
						{
							console->error("输入命令错误：cap + IP");
							continue;
						}
						bool bFindDev = false;
						for (auto&mgr:g_AppConfig.arrLaneSide)
						{
							for (auto&gun:mgr.pTaskMgr->m_pDevMgr->gunDevList)
							{
								if (gun.first == args[1])
								{
									JPEGPic rrImage;
									TimeMeter tm;
									tm.start();
									bool bCap = gun.second->CaptureNEW(rrImage);
									console->info("耗时：{}", tm.stop());
									if (!bCap)
									{
										console->error("Capture失败 {}", args[1]);
										continue;
									}
									DealJPEGPic(args[1], rrImage);
									if (rrImage.buf)
									{
										delete rrImage.buf;
										rrImage.buf = nullptr;
									}
									bFindDev = true;
									break;
								}
							}
							for (auto&gun : mgr.pTaskMgr->m_pDevMgr->iceDevList)
							{
								if (gun.first == args[1])
								{
									JPEGPic rrImage;
									TimeMeter tm;
									tm.start();
									bool bCap = gun.second->TriggerExt();
									bCap = gun.second->CaptureNEW(rrImage);
									console->info("耗时：{}", tm.stop());
									if (!bCap)
									{
										console->error("Capture失败 {}", args[1]);
										continue;
									}
									DealJPEGPic(args[1], rrImage);
									if (rrImage.buf)
									{
										delete rrImage.buf;
										rrImage.buf = nullptr;
									}
									bFindDev = true;
									break;
								}
							}
							for (auto&gun : mgr.pTaskMgr->m_pDevMgr->AxisDevList)
							{
								if (gun.first == args[1])
								{
									Mat mat;
									TimeMeter tm;
									tm.start();
									bool bCap = gun.second->Capture(mat);
									console->info("耗时：{}", tm.stop());
									if (!bCap)
									{
										console->error("Capture失败 {}", args[1]);
										continue;
									}
									DealMat(args[1], mat);
									bFindDev = true;
									break;
								}
							}
						}
						if (!bFindDev)
						{
							console->error("未找到设备 {}", args[1]);
							for (auto&mgr : g_AppConfig.arrLaneSide)
							{
								for (auto&gun : mgr.pTaskMgr->m_pDevMgr->gunDevList)
								{
									console->info("Gun lane[{} {}] ip[{}]", mgr.lane_id, mgr.name, gun.first);
								}
								for (auto&gun : mgr.pTaskMgr->m_pDevMgr->iceDevList)
								{
									console->info("ICE lane[{} {}] ip[{}]", mgr.lane_id, mgr.name, gun.first);
								}
								for (auto&gun : mgr.pTaskMgr->m_pDevMgr->AxisDevList)
								{
									console->info("AXIS lane[{} {}] ip[{}]", mgr.lane_id, mgr.name, gun.first);
								}
							}
						}
                    }
					else if (_stricmp(args[0].c_str(), "task") == 0)
                    {
						if (args.size() != 2 && args.size() != 3 && args.size() != 6 && args.size() != 7)
						{
							console->info("输入命令错误：task + new/cn/pmn/qr/door/seal/imdg/door_open");
							continue;
						}
						if (_stricmp(args[1].c_str(), "new") == 0)
						{
							m_imgList.clear();
							console->debug("清空识别任务");
						}
						else if (_stricmp(args[1].c_str(), "cn") == 0 ||
							_stricmp(args[1].c_str(), "pmn") == 0 ||
							_stricmp(args[1].c_str(), "qr") == 0 ||
							_stricmp(args[1].c_str(), "door") == 0 ||
							_stricmp(args[1].c_str(), "seal") == 0 ||
							_stricmp(args[1].c_str(), "door_open") == 0 ||
							_stricmp(args[1].c_str(), "imdg") == 0)
						{
							vector<tImage> arrImg;
							for (auto &t : m_imgList)
							{
								tImage tImg;
								tImg.file_name = t;
								
								RecogParam rec_param = {};
								console->info("reverse:{} rotate:{}", rec_param.reverse, rec_param.rotate);
								
								if (_stricmp(args[1].c_str(), "cn") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::CN;
								}
								else if (_stricmp(args[1].c_str(), "seal") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::SEAL;
								}
								else if (_stricmp(args[1].c_str(), "imgd") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::IMDG;
								}
								else if (_stricmp(args[1].c_str(), "door") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::DOOR;
								}
								else if (_stricmp(args[1].c_str(), "door_open") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::DOOR_OPEN;
								}
								else if (_stricmp(args[1].c_str(), "pmn") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::PMN;
								}
								else if (_stricmp(args[1].c_str(), "qr") == 0)
								{
									rec_param.rtype = GRpc_RecogParam::PMN_QR;
								}

								if (args.size() == 6)
								{
									rec_param.roi.x = atoi(args[2].c_str());
									rec_param.roi.y = atoi(args[3].c_str());
									rec_param.roi.width = atoi(args[4].c_str());
									rec_param.roi.height = atoi(args[5].c_str());
								}
								else if (args.size() == 7)
								{
									cv::Rect rc;
									rc.x = atoi(args[2].c_str());
									rc.y = atoi(args[3].c_str());
									rc.width = atoi(args[4].c_str());
									rc.height = atoi(args[5].c_str());

									//按ROI截图，重新编码
									cv::Mat img_src = cv::imread(t);
									cv::Mat cut = cv::Mat(img_src, rc);
									
									stringstream ss_cut;
									ss_cut << t;
									ss_cut << args[2] << "_";
									ss_cut << args[3] << "_";
									ss_cut << args[4] << "_";
									ss_cut << args[5] << ".jpg";
									cv::imwrite(ss_cut.str(), cut);
									tImg.file_name = ss_cut.str();
								}
								tImg.arrRecogParam.push_back(rec_param);
								arrImg.push_back(tImg);
							}
							for (auto&img : arrImg)
							{
								//每个图片进行识别，防止测试时文件太多导致CRS异常
								RRIMAGELIST rrImageList;
								DealImg(img, rrImageList);
								for (auto&mgr : g_AppConfig.arrLaneSide)
								{
									if (mgr.pTaskMgr)
									{
										mgr.pTaskMgr->m_RImageList.add(rrImageList);
										break;
									}
								}
							}
						}
                    }
                    else if (_stricmp(args[0].c_str(), "help") == 0 ||
					         _stricmp(args[0].c_str(), "/?") == 0)
					{
						console->info("清空识别任务命令：task + new");
						console->info("发送识别任务命令：task + xxx(cn/pmn/qr/door/seal/imdg/door_open) + [x y w h] + [cut]例：task cn或task cn 0 0 100 200");
						console->info("控制拍摄命令：cap + Ip  例：cap 192.168.1.64");
						console->info("控制录像命令：save + 1/0 + Ip  例：save 1 192.168.1.64(1:开始录像,0:停止录像)");
                        console->info("加载 单张照片命令：read + jpg + path  例：read jpg d://test.jgp");
                        console->info("加载 组作业命令：read + dir + path 例：read dir d://group");
						console->info("加载 单文件夹命令：read + img + path 例：read img d://img");
						console->debug("存储PLC数据命令：record + 1/0  (1:open,0:close)");
						console->debug("显示PLC命令：plc + 1/0  (1:open,0:close)");
						console->debug("读取 PLC原始数据命令：raw + path + 0/1 例：raw d://1.raw 1 (0:不使用PLC数据；1:使用PLC数据)");
						console->debug("显示触发状态指令：state + 1/0  (1:open,0:close)");
						console->debug("旋转图像：rotate + path + angle(正数=逆时针)");
						console->debug("矫正图像：rectify + path + x + y + w + h (矫正图片)");
						console->debug("模拟发送数据：pub + path 例：pub d://1.ini");
						console->debug("FTP上传：ftp + path + FTPPath 例：ftp d://1.ini 20220323/2.ini");
						//console->debug("保存视频：video + ip 例：video 10.10.20.32");
						console->debug("云台get指令：ptz get 摄像机IP地址");
						console->debug("云台set指令：ptz set 摄像机IP地址 P T Z");
						console->debug("模拟车牌结果测试：plate set 摄像机IP地址 图片路径");
						console->debug("模拟作业车道：worklane set 2    ");
						console->debug("模拟着箱：catch set 2   1：着箱 2：非着箱");
						//console->info("模拟发送识别结果：ocrret + 任务号 + 车号 + 箱号 例：ocrret task01 PM12 ABCU123456722G1");
						console->debug("模拟发送识别任务指令：Simulate");
                    }
					else if (_stricmp(args[0].c_str(), "catch") == 0)
					{
					g_testCatch = atoi(args[2].c_str());
					console->debug("模拟着箱 [{}]", g_testCatch);
					}
					else if (_stricmp(args[0].c_str(), "worklane") == 0)
					{
					 g_testLane = atoi(args[2].c_str());
					 console->debug("模拟作业车道 [{}]", g_testLane);
					}
					else if (_stricmp(args[0].c_str(), "taskid") == 0)
					{
					g_testTaskIdMtx.lock();
					g_testTaskId = args[2];
					g_testTaskIdMtx.unlock();
					console->debug("模拟taskid [{}]", g_testLane);
					}
					else if (_stricmp(args[0].c_str(), "plate") == 0)
					{
					
						for (auto&mgr : g_AppConfig.arrLaneSide)
						{
							LicPlateResult result;
							result.nBelieve = 99;
							result.sCamIp = args[2];
							result.sLicPlate = "test";
							result.sFileName = args[3];
							cv::Mat m = cv::imread(args[3]);
							std::shared_ptr<cv::Mat> pimg = std::make_shared<cv::Mat>(m);
							mgr.pTaskMgr->SetPlateResult(result, pimg);
						}
						if (args.size() > 4)
						{
							g_testLane = atoi(args[4].c_str());
							console->debug("模拟作业车道 [{}]", g_testLane);
						}
					}
					else if (_stricmp(args[0].c_str(), "rectify") == 0)
					{
					Mat m = imread(args[1].c_str());
					Mat dst;
					cv::Point2f pos1[4] = {
						cv::Point2f(atoi(args[2].c_str()), atoi(args[3].c_str())),
						cv::Point2f(atoi(args[2].c_str()) + atoi(args[4].c_str()), atoi(args[3].c_str())),
						cv::Point2f(atoi(args[2].c_str()), atoi(args[3].c_str()) + atoi(args[5].c_str())),
						cv::Point2f(atoi(args[2].c_str()) + atoi(args[4].c_str()), atoi(args[3].c_str()) + atoi(args[5].c_str())) };
					cv::Point2f pos2[4] = { cv::Point2f(0,0), cv::Point2f(m.cols,0), cv::Point2f(0,m.rows), cv::Point2f(m.cols,m.rows) };
					cv::Mat re = getPerspectiveTransform(pos1, pos2);
					cv::warpPerspective(m, dst, re, m.size(), cv::INTER_CUBIC);
					string recityimgPath = "./rectify_" + args[2] + "_" + args[3] + "_" + args[4] + "_" + args[5]+".jpg"  ;
					imwrite(recityimgPath, dst);
					console->debug("矫正图片 {} {} {} {} 保存到{}", args[2], args[3], args[4], args[5], recityimgPath);

					}
                    else if (_stricmp(args[0].c_str(), "plc") == 0)
                    {
						if (args.size() != 2)
						{
							console->error("输入命令错误");
							continue;
						}
                        if (args[1] == "1")
                        {
							g_AppConfig.userCfg.bplcshow = true;
							/*PCI_SetShowCount(-1);
							console->debug("PLC显示打开");*/
                        }
                        else if (args[1] == "0")
                        {
							g_AppConfig.userCfg.bplcshow = false;
							/*PCI_SetShowCount(0);
							console->debug("PLC显示关闭");*/
                        }
						else
						{
							/*int nShowCount = atoi(args[1].c_str());
							if (nShowCount > 0)
							{
								PCI_SetShowCount(nShowCount);
								console->debug("PLC显示{}次", args[1]);
							}*/
						}
                    }
					else if (cmd.find("info") == 0)
					{
						if (args.size() != 2)
						{
							console->error("输入命令错误");
							continue;
						}
						g_AppConfig.userCfg.sendInfoShow = atoi(args[1].data());
					}
                    else if (_stricmp(args[0].c_str(), "ptz") == 0)
                    {
						if (args.size() == 3)
						{
							if (_stricmp(args[1].c_str(), "get") == 0)
							{
								bool bFindDev = false;
								for (auto&mgr : g_AppConfig.arrLaneSide)
								{
									for (auto&gun : mgr.pTaskMgr->m_pDevMgr->gunDevList)
									{
										if (gun.first == args[2])
										{
											NET_DVR_PTZPOS ptz;
											gun.second->Get_DVR_Config(ptz);
											// 将Hex转换成十进制
											short P = HexToDec(ptz.wPanPos);
											short T = HexToDec(ptz.wTiltPos);
											short Z = HexToDec(ptz.wZoomPos);

											console->info("get {} PTZ:{} {} {}", args[2], P, T, Z);
											bFindDev = true;
											break;
										}
									}
								}
								if (!bFindDev)
								{
									console->error("未找到设备 {}", args[2]);
									for (auto&mgr : g_AppConfig.arrLaneSide)
									{
										for (auto&gun : mgr.pTaskMgr->m_pDevMgr->gunDevList)
										{
											console->info("Gun lane[{} {}] ip[{}]", mgr.lane_id, mgr.name, gun.first);
										}
									}
								}
							}
							continue;
						}
						if (args.size() == 6)
						{
							bool bFindDev = false;
							for (auto&mgr : g_AppConfig.arrLaneSide)
							{
								for (auto&gun : mgr.pTaskMgr->m_pDevMgr->gunDevList)
								{
									if (gun.first == args[2])
									{
										PTZ pos;
										pos.P = atoi(args[3].c_str());
										pos.T = atoi(args[4].c_str());
										pos.Z = atoi(args[5].c_str());

										if (pos.P < 0)
										{
											pos.P = 3600 + pos.P;
										}
										if (pos.T < 0)
										{
											pos.T = 3600 + pos.T;
										}

										NET_DVR_PTZPOS ptz;
										ptz.wAction = 1;
										ptz.wPanPos = DecToHex(pos.P);
										ptz.wTiltPos = DecToHex(pos.T);
										ptz.wZoomPos = DecToHex(pos.Z);
										gun.second->Set_DVR_Config(ptz);
										console->info("set {} PTZ:{} {} {} ",args[2], pos.P, pos.T, pos.Z);
										bFindDev = true;
										break;
									}
								}
							}
							if (!bFindDev)
							{
								console->error("未找到设备 {}", args[2]);
								for (auto&mgr : g_AppConfig.arrLaneSide)
								{
									for (auto&gun : mgr.pTaskMgr->m_pDevMgr->gunDevList)
									{
										console->info("Gun lane[{} {}] ip[{}]", mgr.lane_id, mgr.name, gun.first);
									}
								}
							}
							continue;
						}
						console->error("云台get指令：ptz get 摄像机IP地址");
						console->error("云台set指令：ptz set 摄像机IP地址 P T Z");
                    }
					/*else if (_stricmp(args[0].c_str(), "ocrret") == 0)
					{
						if (args.size() != 4)
						{
							console->error("模拟发送识别结果：ocrret + 任务号 + 车号 + 箱号 例：ocrret task01 PM12 ABCU123456722G1");
							continue;
						}
						if (args[3].length() != 15)
						{
							console->error("箱号 {} 输入错误", args[3]);
							continue;
						}
						OCRResult ocrr;
						ocrr.set_task_id(args[1]);
						ocrr.set_truck_num(args[2]);
						auto pCn = ocrr.add_cn_arr();
						pCn->set_cnm(args[3].c_str(), 4);
						pCn->set_cnn(args[3].c_str()+4, 7);
						pCn->set_iso(args[3].c_str()+11, 4);

						OCRAny *pCopy = new OCRAny;
						pCopy->mutable_any()->PackFrom(ocrr);
						g_queueSubMsg.add(pCopy);
					}*/
					//else if (_stricmp(args[0].c_str(), "video") == 0)
					//{
					//	console->debug("保存视频功能尚未开发完成");
					//	continue;
					//	if (pManager->gunDevList.find(args[1]) != pManager->gunDevList.end())
					//	{
					//		LONG userId = pManager->gunDevList.find(args[1])->second->GetLoginUserID();
					//		
					//		//创建路径 保存图片
					//		std::string saveVideoPathHolder = args[1];
					//		MakeDirs(saveVideoPathHolder);
					//		std::string videoFileName = saveVideoPathHolder + "/" + GetSysTimeString() + ".mp4";
					//		

					//		long lRealPlayHandle;
					//		NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
					//		struPlayInfo.hPlayWnd = NULL;  // 仅取流不解码。这是Linux写法，Windows写法是struPlayInfo.hPlayWnd = NULL;
					//		struPlayInfo.lChannel = 33; // 通道号
					//		struPlayInfo.dwStreamType = 0;  // 0- 主码流，1-子码流，2-码流3，3-码流4，以此类推
					//		struPlayInfo.dwLinkMode = 0;  // 0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP
					//		struPlayInfo.bBlocked = 0;  // 0- 非阻塞取流，1- 阻塞取流
					//		//struPlayInfo.dwDisplayBufNum = 1;

					//		//lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL); // NET_DVR_RealPlay_V40 实时预览（支持多码流）。
					//		lRealPlayHandle = NET_DVR_RealPlay_V40(userId, &struPlayInfo, NULL, NULL);
					//		bool isSave = NET_DVR_SaveRealData(lRealPlayHandle, (char*)videoFileName.c_str());
					//		console->debug("保存视频：{}，路径：{}  ",isSave, videoFileName);
					//		if (!isSave)
					//		{
					//			console->error("保存视频开始：{}  {}", NET_DVR_GetLastError(), NET_DVR_GetErrorMsg());
					//		}
					//		//默认保存10s,后续可做成命令
					//		int i = 0;
					//		while (i<5)
					//		{
					//			SleepMilliseconds(1000);
					//			i++;
					//		}
					//		isSave = NET_DVR_StopRealPlay(lRealPlayHandle);
					//		if (!isSave)
					//		{
					//			console->error("预览视频结束：{}  {}", NET_DVR_GetLastError(), NET_DVR_GetErrorMsg());
					//		}
					//		isSave = NET_DVR_StopSaveRealData(lRealPlayHandle);
					//		
					//		console->debug("保存视频完成:{}，路径：{}", isSave, videoFileName);
					//		if (!isSave)
					//		{
					//			console->error("保存视频结束：{}  {}", NET_DVR_GetLastError(), NET_DVR_GetErrorMsg());
					//		}
					//	}
					//}
					else if (_stricmp(args[0].c_str(), "pub") == 0)
					{
					if (args.size() != 2)
					{
						console->error("input error,input help for details");
						continue;
					}
					boost::property_tree::ptree pt;
					boost::property_tree::ptree curPt;
					boost::property_tree::ini_parser::read_ini(args[1], pt);
					bool isHaveWorkDataBody = false;
					bool isHaveOcrDataBody = false;
					bool isHaveUnit1 = false;
					bool isHaveUnit2 = false;
					try
					{
						curPt = pt.get_child("WorkDataBody");
						isHaveWorkDataBody = true;
					}
					catch (const std::exception &e)
					{
						console->warn("error 没有WorkDataBody节点");
					}
					try
					{
						curPt = pt.get_child("OcrDataBody");
						isHaveOcrDataBody = true;
					}
					catch (const std::exception &e)
					{
						console->warn("error 没有OcrDataBody节点");
					}
					try
					{
						curPt = pt.get_child("unit_arr1");
						isHaveUnit1 = true;
					}
					catch (const std::exception &e)
					{
						console->warn("error 没有OcrDataBody节点");
					}
					try
					{
						curPt = pt.get_child("unit_arr2");
						isHaveUnit2 = true;
					}
					catch (const std::exception &e)
					{
						console->warn("error 没有OcrDataBody节点");
					}

					OcrData ocrData;
					if (isHaveWorkDataBody)
					{
						WorkDataBody *workData = ocrData.mutable_work_data();
						workData->set_work_id(pt.get<string>("WorkDataBody.work_id", "test_work_id"));
						workData->set_dev_id(pt.get<string>("WorkDataBody.dev_id", "test_dev_id"));
						workData->set_work_lane(pt.get<string>("WorkDataBody.work_lane", "test_dev_id"));
						workData->set_work_type(pt.get<string>("WorkDataBody.work_type", "test_dev_id"));
						workData->set_spreader(pt.get<string>("WorkDataBody.spreader", "test_dev_id"));
						workData->set_work_status(pt.get<int>("WorkDataBody.work_status", 2));
						workData->set_snapshot_location(pt.get<string>("WorkDataBody.snapshot_location", "test_dev_id"));
						workData->set_stay_work(pt.get<int>("WorkDataBody.stay_work", 0));
						/*workData->set_ori_paths(pt.get<string>("WorkDataBody.ori_paths", "test_dev_id"));
						workData->set_seal_paths(pt.get<string>("WorkDataBody.seal_paths", "test_dev_id"));
						workData->set_danger_paths(pt.get<string>("WorkDataBody.danger_paths", "test_dev_id"));
						workData->set_damage_paths(pt.get<string>("WorkDataBody.damage_paths", "test_dev_id"));*/
						//workData->set_transaction_type(pt.get<string>("WorkDataBody.transaction_type", "LDYT"));
					}

					if (isHaveOcrDataBody)
					{
						OcrDataBody *dataBody = ocrData.mutable_ocr_data();
						OcrDataBody::OcrResult *data = dataBody->mutable_original_data();
						data->set_truck_plate(pt.get<string>("OcrDataBody.truck_plate", "test_work_id"));
						data->set_truck_plate_confidence(pt.get<float>("OcrDataBody.truck_plate_confidence", 99.99));
						data->set_truck_head_dir(pt.get<string>("OcrDataBody.truck_head_dir", "U"));
						if (isHaveUnit1)
						{
							OcrDataBody::OcrResult::Unit *unit = data->add_unit_arr();
							unit->set_unit_number(pt.get<string>("unit_arr1.unit_number", "test_work_id"));
							unit->set_unit_number_confidence(pt.get<float>("unit_arr1.unit_number_confidence", 99.99));
							unit->set_unit_number_result(pt.get<int>("unit_arr1.unit_number_result", 1));
							unit->set_iso_code(pt.get<string>("unit_arr1.iso_code", "test_work_id"));
							unit->set_iso_code_confidence(pt.get<float>("unit_arr1.iso_code_confidence", 99.99));
							unit->set_position(pt.get<string>("unit_arr1.position", "test_work_id"));
							unit->set_position_confidence(pt.get<float>("unit_arr1.position_confidence", 99.99));
							unit->set_door_direction(pt.get<string>("unit_arr1.door_direction", "U"));
							unit->set_door_direction_confidence(pt.get<float>("unit_arr1.door_direction_confidence", 99.99));
						}
						if (isHaveUnit2)
						{
							OcrDataBody::OcrResult::Unit *unit = data->add_unit_arr();
							unit->set_unit_number(pt.get<string>("unit_arr2.unit_number", "test_work_id"));
							unit->set_unit_number_confidence(pt.get<float>("unit_arr2.unit_number_confidence", 99.99));
							unit->set_unit_number_result(pt.get<int>("unit_arr2.unit_number_result", 1));
							unit->set_iso_code(pt.get<string>("unit_arr2.iso_code", "test_work_id"));
							unit->set_iso_code_confidence(pt.get<float>("unit_arr2.iso_code_confidence", 99.99));
							unit->set_position(pt.get<string>("unit_arr2.position", "test_work_id"));
							unit->set_position_confidence(pt.get<float>("unit_arr2.position_confidence", 99.99));
							unit->set_door_direction(pt.get<string>("unit_arr2.door_direction", "U"));
							unit->set_door_direction_confidence(pt.get<float>("unit_arr2.door_direction_confidence", 99.99));
						}

					}

					OCRAny any;
					any.mutable_any()->PackFrom(ocrData);
					any.set_msg_id(GetUUID());
					any.set_msg_type(MSG_TYPE::MSG_OCRDATA);
					any.set_src_id(g_AppConfig.userCfg.IARSName);
					any.set_time(GetTimeString());
					any.set_need_reply(1);
					any.set_dev_id(g_AppConfig.userCfg.quayNo);

					g_WaitPubMsg.add(any);
					}
					else if (_stricmp(args[0].c_str(), "state") == 0)
					{
						if (args.size() == 2)
						{
							g_AppConfig.userCfg.bPlcState = atoi(args[1].c_str()) != 0;
							console->info("Plc State Show {}", g_AppConfig.userCfg.bPlcState ? "open" : "close");
						}
						else
							console->error("input error,input help for details");
					}
					else if (_stricmp(args[0].c_str(), "raw") == 0)
					{
						if (args.size() < 2)
						{
							console->error("input error,input help for details");
						}
						else
						{
							//0：只显示PLC数据；1：使用plc触发条件 默认为1；
							bool isUsePlc = false;
							if (args.size() == 2)
							{
								isUsePlc = true;
							}
							else
							{
								if (args[2] == "1")
								{
									isUsePlc = true;
								}
								else if (args[2] == "0")
								{
									isUsePlc = false;
								}
								else
								{
									console->error("input error,input help for details");
									isUsePlc = false;
								}
							}
							try
							{
								boost::filesystem::ifstream ifs;
								ifs.open(args[1].c_str(), std::ios_base::binary | std::ios_base::in);
								if (ifs.is_open())
								{
									console->info("start read plc data");
									while (!ifs.eof())
									{
										PLCInfo plcInfo;
										PLCMESSAGE plcmsg;
										ifs.read((char*)&plcmsg, sizeof(plcmsg));
										//plcInfo.ParseMsg(plcmsg);
										if (isUsePlc)
										{
											for (auto& t : g_plcSubscriber)
											{
												if (t.second)
												{
													*(t.second) = plcInfo;
												}
											}
											// 通知PLC数据更新
											boost::unique_lock < boost::mutex> lck(g_PlcUpdateMutex);
											g_PlcUpdateCond.notify_all(); // 唤醒所有线程.
										}
										
										if (g_AppConfig.userCfg.bplcshow)
										{
											console->warn("{}", plcInfo);
										}
										
										std::this_thread::sleep_for(std::chrono::milliseconds(100));
									}
									ifs.close();
									console->info("finish read plc data");
								}
							}
							catch (std::exception& e)
							{
								console->error("open {} fail:{}", args[1], e.what());
							}
							catch (...)
							{
							}
						}
					}
					/*else if (_stricmp(args[0].c_str(), "save") == 0)
					{
						if (args.size() == 3)
						{
							auto itGun = pManager->gunDevList.find(args[2]);
							if (itGun != pManager->gunDevList.end())
							{
								bool bOpen = atoi(args[1].c_str()) != 0;
								itGun->second->SaveRealData(bOpen);
							}
							else
							{
								console->error("未找到[{}]相机", args[2]);
							}
						}
						else
						{
							console->error("input error,input help for details");
						}
					}*/
					else if (_stricmp(args[0].c_str(), "rotate") == 0)
					{
						if (args.size() == 3)
						{
							cv::Mat img = cv::imread(args[1]);
							if (!img.empty())
							{
								int angle = atoi(args[2].c_str());
								cv::Mat dst;
								set_rotate_angle(img, dst, angle);
								string dst_path = args[1] + "_" + args[2] + ".jpg";
								cv::imwrite(dst_path, dst);
								console->debug("图片旋转完成 {}", dst_path);
							}
							else
							{
								console->error("图片读取失败 {}", args[1]);
							}
						}
						else
						{
							console->error("input error,input help for details");
						}
					}
					else if (_stricmp(args[0].c_str(), "ftp") == 0)
					{
						if (args.size() == 3)
						{
							CFtpClient::getInstence().UploadFile(args[1], args[2]);
						}
						else
						{
							console->error("input error,input help for details");
						}
					}
					else if (_stricmp(args[0].c_str(), "write") == 0)
					{
						try
						{
							boost::property_tree::ptree pt;
							boost::property_tree::ini_parser::read_ini("data_to_plc.ini", pt);
							g_mtxOCRToPlc.lock();
							g_OCRToPlc.taskId = pt.get<int>("PLC.WorkId", 0);
							g_OCRToPlc.resultType = pt.get<int>("PLC.ResultType", 0);
							g_OCRToPlc.usContCount = pt.get<int>("PLC.ContCount", 0);
							g_OCRToPlc.strTruckNo = pt.get<string>("PLC.TruckNo", "");
							g_OCRToPlc.strContNo = pt.get<string>("PLC.ContNo", "");
							g_OCRToPlc.strContIso = pt.get<string>("PLC.ContIso", "");
							g_OCRToPlc.strTruckNo = pt.get<string>("PLC.TruckNo", "");
							g_OCRToPlc.strPlate = "皖" + pt.get<string>("PLC.Plate", "");
							g_OCRToPlc.doorOpen = pt.get<int>("PLC.DoorOpen", 0);
							g_OCRToPlc.doorDir = pt.get<int>("PLC.DoorDir", 0);
							string strPlc = g_OCRToPlc.to_string();
							g_mtxOCRToPlc.unlock();

							console->info("模拟发送数据到PLC {}", strPlc);
							
						}
						catch (std::exception& e)
						{
							console->error("write命令执行异常：{}", e.what());
						}
						catch (...)
						{
							console->error("write命令执行异常");
						}
					}
					else if (_stricmp(args[0].c_str(), "Simulate") == 0)
					{
					Simulate();
					}
					else
					{
						console->error("invalid input command,input help for details");
					}
                };
            }
        };
        return;
    }

    void Stop()
    {
        exitFlag = true;
        worker.detach();
        console->info("调试模块退出");
    }

	void DealJPEGPic(string camip, JPEGPic& rrImage)
	{
		cv::Mat rgb;
		cv::Mat jpeg(1, rrImage.size, CV_8UC1, rrImage.buf);
		cv::imdecode(jpeg, cv::IMREAD_COLOR, &rgb);

		//创建路径 保存图片
		std::string saveImgPathHolder = g_AppConfig.arrLaneSide[0].pTaskMgr->m_TaskInfo.local_snapshot_location();
		console->info("保存截取图片，路径1：{}", saveImgPathHolder);
		MakeDirs(saveImgPathHolder);
		std::string imgFileName = saveImgPathHolder + "/" + GetSysTimeString() + ".jpg";
		cv::imwrite(imgFileName, rgb);
		console->info("保存截取图片，路径2：{}", imgFileName);

		shared_ptr<GRpc_RImage> pRImg = make_shared<GRpc_RImage>();
		GRpc_RImage &img = *pRImg;
		img.mutable_image()->set_width(rgb.cols);
		img.mutable_image()->set_height(rgb.rows);
		img.mutable_image()->set_channel(rgb.channels());
		//img.mutable_image()->set_type(GRpc_Image::BACK_DOOR); //后箱门
		img.mutable_image()->set_data(rrImage.buf, rrImage.size);
		img.set_img_label(time(nullptr));

		GRpc_RecogParam * param = img.add_param();
		param->set_type(GRpc_RecogParam::G_RType(1));
		param->mutable_roi()->set_x(0);
		param->mutable_roi()->set_y(0);
		param->mutable_roi()->set_w(rgb.cols);
		param->mutable_roi()->set_h(rgb.rows);

		RRIMAGELIST rrImageList;
		rrImageList.push_back(pRImg);
	}
	void DealMat(string camip, Mat& mat)
	{
		cv::Mat rgb;
		cv::imdecode(mat, cv::IMREAD_COLOR, &rgb);

		std::vector<uchar> buf;
		buf.resize(MAX_JPEG_BUFSIZE);
		cv::imencode(".jpg", mat, buf);

		//创建路径 保存图片
		std::string saveImgPathHolder = camip;
		MakeDirs(saveImgPathHolder);
		std::string imgFileName = saveImgPathHolder + "/" + GetSysTimeString() + ".jpg";
		cv::imwrite(imgFileName, rgb);
		console->info("保存截取图片，路径：{}", imgFileName);

		shared_ptr<GRpc_RImage> pRImg = make_shared<GRpc_RImage>();
		GRpc_RImage &img = *pRImg;
		img.mutable_image()->set_width(rgb.cols);
		img.mutable_image()->set_height(rgb.rows);
		img.mutable_image()->set_channel(rgb.channels());
		//img.mutable_image()->set_type(GRpc_Image::BACK_DOOR); //后箱门
		img.mutable_image()->set_data(&buf[0], buf.size());
		img.set_img_label(time(nullptr));

		GRpc_RecogParam * param = img.add_param();
		param->set_type(GRpc_RecogParam::G_RType(1));
		param->mutable_roi()->set_x(0);
		param->mutable_roi()->set_y(0);
		param->mutable_roi()->set_w(rgb.cols);
		param->mutable_roi()->set_h(rgb.rows);

		RRIMAGELIST rrImageList;
		rrImageList.push_back(pRImg);
	}
	void ReadImageNode(ptree &node, vector<tImage>& arrImg)
	{
		tImage img;
		img.file_name = node.get<std::string>("<xmlattr>.file_name");
		img.cam_ip = node.get<std::string>("<xmlattr>.cam_ip");
		img.cam_pos = node.get<std::string>("<xmlattr>.cam_pos");
		auto childs = node.get_child("RecogType");
		for (auto&child : childs)
		{
			RecogParam param;
			param.name = child.first;
			auto str = child.second.get<std::string>("<xmlattr>.rtype");
			if (str.length())
				param.rtype = (ocrs::pb::GRpc_RecogParam::G_RType)atoi(str.c_str());

			param.rectify = child.second.get<std::string>("<xmlattr>.rectify", "");
			param.reverse = child.second.get<bool>("<xmlattr>.reverse", false);
			param.rotate = child.second.get<int>("<xmlattr>.rotate", 0);

			str = child.second.get<std::string>("<xmlattr>.roi");
			//--jcy add  Judge include 3 ","
			if (str.length())
			{
				int num = 0;
				for (int i = 0; i < str.length(); i++)
				{
					if (str[i] == ',' && str[i - 1] != ',')
					{
						num++;
					}
				}
				if (num == 3)
				{
					param.roi = str2rect(str.c_str());
				}
				else
				{
					console->error("ROI[{}]格式错误(x,y,w,h)", str);
					std::this_thread::sleep_for(std::chrono::seconds(5));
					std::exit(1);//节点有错，直接退出
				}
			}
			//--end
			param.roi_str = str;
			img.arrRecogParam.push_back(param);
		}
		arrImg.push_back(img);
	}
	void DealImg(tImage& img, RRIMAGELIST& rrImageList)
	{
		string file_path = img.file_name;
		JPEGPic rrImage;
		bool ret = ReadImage(file_path.c_str(), rrImage.buf, rrImage.size);
		if (!ret)
		{
			console->error("读取图片失败 {}", file_path);
			return;
		}
		
		cv::Mat jpeg(1, rrImage.size, CV_8UC1, rrImage.buf);
		shared_ptr<cv::Mat> pImg = make_shared<cv::Mat>();
		cv::imdecode(jpeg, cv::IMREAD_COLOR, pImg.get());

		for (auto &rp: img.arrRecogParam)
		{
			shared_ptr<cv::Mat> pImgCvt = nullptr;
			string strConvertName;
			try
			{
				if (rp.rectify.length())
				{
					console->warn("文件[{}]设置rectify[{}]", file_path, rp.rectify);
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
						console->warn("文件[{}]设置rectify成功", file_path);
					}
					else
						console->warn("文件[{}]设置rectify[{}]无效，超出[0,0,{},{}]范围", 
							file_path, rp.rectify, pImg->cols, pImg->rows);
				}
				if (rp.reverse)
				{
					console->warn("文件[{}]设置reverse", file_path);
					if (!pImgCvt)
						pImgCvt = make_shared<cv::Mat>(*pImg);
					UnevenLightCompensate(*pImgCvt);
					strConvertName += "_reverse";
				}

				if (rp.rotate)
				{
					try
					{
						console->warn("文件[{}]设置图像旋转[{}]", file_path, rp.rotate);
						if (!pImgCvt)
							pImgCvt = make_shared<cv::Mat>(*pImg);
						set_rotate_angle(*pImg, *pImgCvt, rp.rotate);
						strConvertName += "_rotate_" + to_string(rp.rotate);
					}
					catch (std::exception& e)
					{
						console->error("图像旋转异常：{}", e.what());
					}
					catch (...)
					{
						console->error("图像旋转异常");
					}
				}
			}
			catch (std::exception& e)
			{
				console->error("文件[{}]处理异常{}", file_path, e.what());
			}
			catch (...)
			{
				console->error("文件[{}]处理异常", file_path); 
			}

			//测试的img_label从2000开始，与实际作业（0~1000）进行区分
			static int s_img_label = 2000;
			tPicItem item;
			item.pTaskMgr = g_AppConfig.arrLaneSide[0].pTaskMgr.get();
			item.cam_name = img.cam_pos;
			item.cam_ip = img.cam_ip;
			item.group = 0;
			item.task_id = g_AppConfig.arrLaneSide[0].pTaskMgr->m_task_id;
			item.img_label = s_img_label++;

			string save_name = to_string(item.img_label) + "_";
			for (int i = img.file_name.length(); i > 0; i--)
			{
				if (img.file_name[i] == '/' || img.file_name[i] == '\\')
				{
					save_name += img.file_name.c_str() + i + 1;
					break;
				}
			}
			item.ori_name = save_name;
			item.ori_img_name = save_name + "_ORI.jpg";
			item.pImgOri = pImg;

			std::shared_ptr<ocrs::pb::GRpc_RImage> pRImage = std::make_shared<ocrs::pb::GRpc_RImage>();
			pRImage->set_img_label(item.img_label);
			pRImage->set_img_type(img.cam_pos);
			pRImage->set_cam_id(img.cam_ip);
			pRImage->mutable_image()->set_width(pImg->cols);
			pRImage->mutable_image()->set_height(pImg->rows);
			pRImage->mutable_image()->set_channel(pImg->channels());
			JPEGPic rrCvt;
			if (pImgCvt)
			{
				string cvtImgName = save_name + strConvertName + "_CVT.jpg";
				//进行旋转后，图像尺寸可能会改变，再次赋值
				pRImage->mutable_image()->set_width(pImgCvt->cols);
				pRImage->mutable_image()->set_height(pImgCvt->rows);
				item.pImgCvt = pImgCvt;

				Mat2JPEG(*pImgCvt, rrCvt);

				item.cvt_path = g_AppConfig.arrLaneSide[0].pTaskMgr->m_TaskInfo.local_snapshot_location() + cvtImgName;
				//保存图片
				cv::imwrite(item.cvt_path, *pImgCvt);
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
			item.arrParam.push_back(pic_param);

			console->info("文件[{}]发布识别照片\n{}", file_path, ConvertChs(pRImage->DebugString()));

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
				pRImage->mutable_image()->set_data(rrImage.buf, rrImage.size);
			rrImageList.push_back(pRImage);
		}
		if (rrImage.buf)
		{
			delete[]rrImage.buf;
			rrImage.buf = nullptr;
		}
	}
	//Mat转buffer
	void Mat2JPEG(const cv::Mat &srcMat, JPEGPic &capPic)
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
	void Simulate()
	{
		vector<tImage> arrImg;
		try
		{
			ptree xmlRootNode;
			read_xml("./SimulateConfig.xml", xmlRootNode);
			auto child = xmlRootNode.get_child("GalaxyVism");

			for (auto pNode = child.begin(); pNode != child.end(); ++pNode)
			{
				// 节点信息
				if (pNode->first == "img")
				{
					ReadImageNode(pNode->second, arrImg);
				}
			}
		}
		catch (std::exception& e)
		{
			console->error("Load SimulateConfig.xml fail:{}", e.what());
			return;
		}
		catch (...)
		{
			console->error("Load SimulateConfig.xml fail");
			return;
		}
		
		for (auto&img : arrImg)
		{
			//每个图片进行识别，防止测试时文件太多导致CRS异常
			RRIMAGELIST rrImageList;
			DealImg(img, rrImageList);
			g_AppConfig.arrLaneSide[0].pTaskMgr->m_RImageList.add(rrImageList);
		}
	}
private:
    std::thread worker;  // 工作线程
    bool exitFlag;

};


#endif //DEBUG_H