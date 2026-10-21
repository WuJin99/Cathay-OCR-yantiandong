#include "PLCHttpComm.h"
#include "AppConfig.h"
#include "Console.h"
#include "PLCInfo.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
OCRToSANY g_OCRToSANY;
std::mutex g_OCRToSANYMtx;
using json = nlohmann::json;
PLCHttpComm::PLCHttpComm(boost::shared_ptr<TaskManager> taskMgr)
{
	m_pTaskMgr = taskMgr;
	svrptr = std::make_shared<httplib::Server >();
}

PLCHttpComm::~PLCHttpComm()
{
}
void PLCHttpComm::Stop()
{
	m_run = false;
	cliptr->stop();
	svrptr->stop();
}
void PLCHttpComm::SendDoorDirData()
{
	if (cliptr)
	{
		m_pTaskMgr->m_plcinfoMtx.lock();
		PLCInfo plc = m_pTaskMgr->m_plcinfo;
		int doorcode = m_pTaskMgr->Sany_Code.door_code;
		m_pTaskMgr->m_plcinfoMtx.unlock();

		int nDoorDirRet = m_pTaskMgr->GetDoorDirCorrect();
		boost::property_tree::ptree pt;
		pt.put("UUID", GetUUID());
		pt.put("Timestamp", GetHttpTimeString());
		pt.put("TaskId", plc.taskId);
		pt.put("CraneId", plc.craneId);
		pt.put("Orientation", nDoorDirRet);

		pt.put("StatusCode", doorcode);
		std::stringstream sss;
		boost::property_tree::json_parser::write_json(sss, pt);

		if (g_OCRToSANY.doorDir != nDoorDirRet)
		{
			g_OCRToSANY.doorDir = nDoorDirRet;
			AddMainLog(fmt::format("发送箱门方向，Orientation:1=succ,2=fail {}", sss.str()));
		}
		if (g_OCRToSANY.door_code != doorcode)
		{
			g_OCRToSANY.door_code = doorcode;
			AddMainLog(fmt::format("发送箱门方向，Orientation:1=succ,2=fail {}", sss.str()));
		}

		console->info("[door] send doordir result [{}] json:[{}]", nDoorDirRet, sss.str());
		if (auto res = cliptr->Post(g_AppConfig.plcCfg.plcHttpCfg.doorDirPath, sss.str(),"application/json"))
		{
			console->info("[door] send doordir result result status[{}]", res->status);
			if (res->status == 200)
			{
				m_pTaskMgr->AddTaskLog(console->info("PLC HTTP request[{}] success  doordir [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorDirPath, nDoorDirRet));
			}
			else
			{
				m_pTaskMgr->AddTaskLog(console->error("PLC HTTP request [{}] error status [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorDirPath, res->status));
			}
		}
		else
		{
			auto err = res.error();
			console->error("PLC HTTP request[{}] error code [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorDirPath, int(err));
		}
	}
	else
	{
		console->error("PLC HTTP request [{}] error",g_AppConfig.plcCfg.plcHttpCfg.doorDirPath );
	}
}
void PLCHttpComm::SendDoorOpenData()
{
	if (cliptr)
	{
		m_pTaskMgr->m_plcinfoMtx.lock();
		PLCInfo plc = m_pTaskMgr->m_plcinfo;
		int opencode = m_pTaskMgr->Sany_Code.open_code;
		m_pTaskMgr->m_plcinfoMtx.unlock();

		boost::property_tree::ptree ptt;
		ptt.put("UUID", GetUUID());
		ptt.put("Timestamp", GetHttpTimeString());
		ptt.put("TaskId", plc.taskId);
		ptt.put("CraneId", plc.craneId);
		ptt.put("IsOpen", bool(m_pTaskMgr->m_bDoorOpenResult));
		//ptt.put("IsOpen", true);
		ptt.put("StatusCode", opencode);
		std::stringstream ss;
		boost::property_tree::json_parser::write_json(ss, ptt);

		int nOpenRet = m_pTaskMgr->m_bDoorOpenResult ? 1 : 0;
		if (g_OCRToSANY.doorOpen != nOpenRet)
		{
			g_OCRToSANY.doorOpen = nOpenRet;
			AddMainLog(fmt::format("发送箱门打开， {}", ss.str()));
		}
		if (g_OCRToSANY.open_code != opencode)
		{
			g_OCRToSANY.open_code = opencode;
			AddMainLog(fmt::format("发送箱门打开， {}", ss.str()));
		}

		console->info("[door] send dooropen result [{}] json:[{}]", m_pTaskMgr->m_bDoorOpenResult, ss.str());
		if (auto res = cliptr->Post(g_AppConfig.plcCfg.plcHttpCfg.doorOpenPath, ss.str(), "application/json"))
		{
			console->info("[door] send doorOpen result status[{}]", res->status);
			if (res->status == 200)
			{
				m_pTaskMgr->AddTaskLog( console->info("PLC HTTP request[{}] success  doorOpen [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorOpenPath, m_pTaskMgr->m_bDoorOpenResult));
			}
			else
			{
				m_pTaskMgr->AddTaskLog(console->error("PLC HTTP request [{}] error status [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorOpenPath, res->status));
			
				console->info("[door] send doorOpen result body[{}]", res->body);
			}
		}
		else
		{
			auto err = res.error();
			console->error("PLC HTTP request[{}] error code [{}]", g_AppConfig.plcCfg.plcHttpCfg.doorOpenPath, int(err));
		}
	}
	else
	{
		console->error("PLC HTTP request [{}] error", g_AppConfig.plcCfg.plcHttpCfg.doorOpenPath);
	}
}
void PLCHttpComm::SendOCRHeartBeat()
{
	if (cliptr)
	{
		//console->info("[ocr] send heartbeat");
		if (auto res = cliptr->Post("/api/piis/ocr/heartbeat", "", "application/json"))
		{
			console->info("[ocr] send heartbeat status[{}]", res->status);
			if (res->status == 200)
			{
				//console->info("[ocr] send heartbeat");
			}
			else
			{
				console->info("[ocr] send heartbeat body[{}]", res->body);
			}
		}
		else
		{
			auto err = res.error();
			console->error("PLC HTTP request[/api/piis/ocr/heartbeat] error code [{}]", int(err));
		}
	}
	else
	{
		console->error("PLC HTTP request [/api/piis/ocr/heartbeat] error");
	}
}
void PLCHttpComm::SendOCRSystemState()
{
	if (cliptr)
	{		
		boost::property_tree::ptree ptState;
		ptState.put("State", int(g_OcrSystemState));
		ptState.put("DisconnIp", g_strDisconnCameraIp);

		std::stringstream sssState;
		boost::property_tree::json_parser::write_json(sssState, ptState);
		console->info("[ocr] send systemstate json:[{}]", sssState.str());
		if (auto res = cliptr->Post("/api/piis/ocr/system-state", sssState.str(), "application/json"))
		{
			console->info("[ocr] send systemstate status[{}]", res->status);
			if (res->status == 200)
			{
				//console->info("[ocr] send heartbeat");
			}
			else
			{
				console->info("[ocr] send systemstate body[{}]", res->body);
			}
		}
		else
		{
			auto err = res.error();
			console->error("PLC HTTP request[/api/piis/ocr/system-state] error code [{}]", int(err));
		}
	}
	else
	{
		console->error("PLC HTTP request [/api/piis/ocr/system-state] error");
	}
}
bool PLCHttpComm::Start()
{
	m_run = true;
	cliptr = std::make_shared<httplib::Client>(g_AppConfig.plcCfg.plcHttpCfg.remoteIP, g_AppConfig.plcCfg.plcHttpCfg.remotePort);
	cliptr->set_connection_timeout(3);//设置超时连接时间3s
	cliptr->set_read_timeout(3);//设置读取超时时间3s

	std::thread work = std::thread
	{
		[](PLCHttpComm *plc)
		{
			int count = 0;
			int lastTaskState = 0;
			string lastTaskId = "";
			while (plc->m_run)
			{
				if (plc->cliptr)
				{
					if (auto res = plc->cliptr->Get(g_AppConfig.plcCfg.plcHttpCfg.plcPath))
					{
						if (res->status == 200)
						{
							std::istringstream json_stream(res->body);
							boost::property_tree::ptree pt;
							try 
							{
								boost::property_tree::read_json(json_stream, pt);
								std::string value = pt.get<std::string>("isSuccess");
								//console->info("plc read [{}]", value);
								//std::istringstream json_streams ( pt.get<std::string>("data"));
								ptree attr = pt.get_child("data");
								PLCInfo plc_info;
								plc_info.ParseMsg(attr);
								if (g_AppConfig.userCfg.bplcshow)
								{
									console->warn("[plcinfo] {}",plc_info);
								}
								//任务状态/任务ID改变时，显示接收到的原始数据及解析后的PLC_INFO
								if (lastTaskState != plc_info.taskState)
								{
									lastTaskState = plc_info.taskState;
									AddMainLog(console->debug("[support-data] task state change {}", res->body));
									g_OCRToSANY.Clear();
									if (!g_AppConfig.userCfg.bplcshow)
									{
										console->warn("[plcinfo] {}", plc_info);
									}
								}
								if (lastTaskId != plc_info.taskId)
								{
									/*
									任务状态为激活，任务号发生变化时将任务状态置为结束，TaskManager::TaskTrigger函数将当前任务结束
									箱号识别上个任务箱号情况经过检查日志得出三一上位机未给我方发送任务结束状态导致OCR默认为任务没有结束。
									已联系三一上位机给我方的解释为任务完成之后可能下一个指令立马就来了，单纯监控任务状态字段不能知晓是否是任务，需要监控TaskId的变更。
									*/
									/*if (plc_info.taskState == 1)
									{
										plc_info.taskState = 99;
										console->warn(" ******* 作业结束，任务激活且任务号变化{} -> {} *******", lastTaskId, plc_info.taskId);
									}*/
									lastTaskId = plc_info.taskId;
									AddMainLog(console->debug("[support-data] task id change {}", res->body));
									g_OCRToSANY.Clear();
									if (!g_AppConfig.userCfg.bplcshow)
									{
										console->warn("[plcinfo] {}", plc_info);
									}
								}

								for (auto& t : g_plcSubscriber)
								{
									if (t.second)
									{
										*t.second = plc_info;
									}
								}
								// 通知PLC数据更新
								boost::mutex::scoped_lock lock(g_PlcUpdateMutex);
								g_PlcUpdateCond.notify_all();

							}
							catch (std::exception& e) {
								console->error("PLC HTTP request Exception: [{}]" , e.what() );
								
							}
						}
						else
						{
							g_OcrSystemState = OCR_COM_FAULT;
							console->error("PLC HTTP request error status [{}]", res->status);
						}
					}
					else
					{
						auto err = res.error();
						console->error("PLC HTTP request error code [{}]",int(err));
					}
				}
				else
				{
					console->error("PLC HTTP request error");
				}

				if (plc->m_pTaskMgr->m_bDoorDirReNeed && plc->m_pTaskMgr->m_taskState == 1)
				{
					console->info("[door] send doordir result==================");
					plc->SendDoorDirData();
					plc->SendDoorOpenData();
					console->info("[door] send doordir result==================");
					plc->m_pTaskMgr->m_bDoorDirReNeed = false;
				}
				if (plc->m_pTaskMgr->m_bDoorOpenReNeed && plc->m_pTaskMgr->m_taskState == 1)
				{
					console->info("[door] send dooropen result==================");
					plc->SendDoorOpenData();
					plc->m_pTaskMgr->m_bDoorOpenReNeed = false;
					console->info("[door] send dooropen result==================");
				}

				
				//发送心跳,10*200ms
				if (count > 10)
				{
					for (auto&gun : plc->m_pTaskMgr->m_pDevMgr->gunDevList)
					{
						if (gun.second->stateType == StateInfo::RUN_STATE::StateInfo_RUN_STATE_RUN_FAULT)
						{
							g_OcrSystemState = OCR_CAM_FAULT;
							if (g_strDisconnCameraIp.length() != 0)
							{
								g_strDisconnCameraIp.append(";");
							}
							g_strDisconnCameraIp.append(gun.second->GetDeviceIP());

							console->error("上报前检查: OCRSystemState[{}],HKCAM[{}] offline", g_OcrSystemState, gun.second->GetDeviceIP());
						}
					}
					for (auto&ice : plc->m_pTaskMgr->m_pDevMgr->iceDevList)
					{
						if (ice.second->isOnlineState == false)
						{
							g_OcrSystemState = OCR_CAM_FAULT;
							if (g_strDisconnCameraIp.length() != 0)
							{
								g_strDisconnCameraIp.append(";");
							}
							g_strDisconnCameraIp.append(ice.second->GetDeviceIP());

							console->error("上报前检查: OCRSystemState[{}],ICECAM[{}] offline", g_OcrSystemState, ice.second->GetDeviceIP());
						}

					}
					for (auto&axis : plc->m_pTaskMgr->m_pDevMgr->AxisDevList)
					{
						if (axis.second->stateType == StateInfo::RUN_STATE::StateInfo_RUN_STATE_RUN_FAULT)
						{
							g_OcrSystemState = OCR_CAM_FAULT;
							if (g_strDisconnCameraIp.length() != 0)
							{
								g_strDisconnCameraIp.append(";");
							}
							g_strDisconnCameraIp.append(axis.second->GetDeviceIP());

							console->error("上报前检查: OCRSystemState[{}],AXISCAM[{}] offline", g_OcrSystemState, axis.second->GetDeviceIP());
						}
					}
					for (auto&axis : plc->m_pTaskMgr->m_pDevMgr->dhDevList)
					{
						if (axis.second->stateType == StateInfo::RUN_STATE::StateInfo_RUN_STATE_RUN_FAULT)
						{
							g_OcrSystemState = OCR_CAM_FAULT;
							if (g_strDisconnCameraIp.length() != 0)
							{
								g_strDisconnCameraIp.append(";");
							}
							g_strDisconnCameraIp.append(axis.second->GetDeviceIP());

							console->error("上报前检查: OCRSystemState[{}],DHCAM[{}] offline", g_OcrSystemState, axis.second->GetDeviceIP());
						}
					}

					plc->SendOCRHeartBeat();

					//断线相机IP为空，表示正常
					if (g_strDisconnCameraIp.empty()&& g_OcrSystemState != OCR_CRS_DISCONNECT)
						g_OcrSystemState = OCR_SYS_NORMAL;
					plc->SendOCRSystemState();
					// 清空断连IP字符串，放在这避免断连IP重复叠加
					g_strDisconnCameraIp.clear();
					count = 0;
				}
				else
				{
					count++;
				}
				Sleep(g_AppConfig.plcCfg.plcHttpCfg.waitTime);
			}
		}, this
	};
	work.detach();

	svrptr->Get(g_AppConfig.plcCfg.plcHttpCfg.cnDataPath,
			[&](const httplib::Request &req, httplib::Response &res)
			{
				console->info("[request] cn body:{}", req.body);
				/*json jsonValue;
				m_pTaskMgr->GetCnResult(jsonValue);
				
				jsonValue["IsSuccess"] = true;
				jsonValue["Message"] = "OK";

				string str_json = jsonValue;*/

				string str_json;
				m_pTaskMgr->GetCnResult(str_json);
				res.set_content(str_json, "application/json;charset=utf-8");

				console->info("发送CnResult {}", str_json);
			}
		);

		svrptr->Get(g_AppConfig.plcCfg.plcHttpCfg.pmnDataPath,
			[&](const httplib::Request &req, httplib::Response &res)
			{
				console->info("[request] pmn body:{}", req.body);
				/*json jsonValue;
				m_pTaskMgr->GetPmnResult(jsonValue);
				jsonValue["IsSuccess"] = true;
				jsonValue["Message"] = "OK";

				string str_json = jsonValue;*/
				string str_json; 
				m_pTaskMgr->GetPmnResult(str_json);
				res.set_content(str_json, "application/json;charset=utf-8");
				console->info("发送PmnResult {}", str_json);
			}
		);
		svrptr->listen(g_AppConfig.plcCfg.plcHttpCfg.localHostIP, g_AppConfig.plcCfg.plcHttpCfg.serverPort);

		return true;
}
