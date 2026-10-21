#include "CameraConfig.h"
#include "Console.h"

LaneConfig g_LaneCfg;
PositionConfig g_PositionCfg;
std::mutex g_PositionCfgMutex;

bool Config::load(const std::string xmlPath)
{
    try
    {
		ptree xmlRootNode;
		read_xml(xmlPath, xmlRootNode);
		auto pNode = xmlRootNode.get_child("GalaxyVism");

		// 根节点 
		if (pNode.size() > 0)
		{
			// 父节点 
			if (groupName.length() != 0)
			{
				pNode = pNode.get_child(groupName);

				if (pNode.size() > 0)
				{
					// 子节点 
					if (nodeName.length() != 0)
					{
						pNode = pNode.get_child(nodeName);
					}
				}
			}
			console->info("load {} {}", groupName, nodeName);
			return deserialize(nodeName, pNode);
		}
    }
    catch (std::exception& e)
    {
		console->error("load [{}] error:{}", xmlPath, e.what());
    }
    catch (...)
    {
		console->error("load [{}] error", xmlPath);
    }
    return false;
}
bool TaskEventConfig::deserialize(std::string node_name, const ptree pNode)
{
	eventList.clear();

	if (pNode.size() > 0)
	{
		try
		{
			for (auto &t : pNode)
			{
				if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
				{
					ptree childNode;
					childNode = pNode.get_child(t.first);
					// 跳过无效的事件
					try
					{
						Event evt;
						if (evt.deserialize(t.first, childNode))
						{
							eventList[t.first] = evt;
							console->info("load {} {} ok", node_name, t.first);
						}
						else 
						{
							console->error("load {} {} fail", node_name, t.first);
						}
					}
					catch (std::exception& e)
					{
						console->error("{} {}", t.first, e.what());
						std::exit(1);//节点有错，直接退出
					}
				}
			}
			return true;
		}
		catch (const std::exception&e)
		{
			console->error("TaskEventConfig error:{}", e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{
			console->error("TaskEventConfig error");
			std::exit(1);//节点有错，直接退出
		}
	}
	return false;
}

bool IPCConfig::deserialize(std::string node_name, ptree pNode)
{
    if (pNode.size() > 0)
    {
		try 
		{
			cnFirst = pNode.get<bool>("<xmlattr>.cnFirst", false);
			camName = pNode.get<std::string>("<xmlattr>.camName");

			ip = pNode.get<std::string>("<xmlattr>.ip");

			nPort = pNode.get<int>("<xmlattr>.nPort");
			lane = pNode.get<int>("<xmlattr>.lane");

			userName = pNode.get<std::string>("<xmlattr>.userName");

			passwd = pNode.get<std::string>("<xmlattr>.passwd");

			usable = lexical_cast<bool>(pNode.get<int>("<xmlattr>.usable"));

			return true;
		}

		catch (const std::exception&e)
		{
			console->error("HKIPCConfig {} {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{

		}
    }
    return false;
}

bool HKGunConfig::deserialize(std::string node_name, const ptree pNode)
{
	if (IPCConfig::deserialize(node_name, pNode))
	{
		try 
		{
			// 事件
			ptree eventsNode;
			nodeName = "Events";
			eventsNode = pNode.get_child(nodeName);
			events.deserialize(nodeName, eventsNode);
			return true;
		}
		catch (const std::exception&e)
		{
			console->error("Gun {} {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{

		}
	}
	return false;
}
//大华
bool DHCamConfig::deserialize(std::string node_name, const ptree pNode)
{
	if (IPCConfig::deserialize(node_name, pNode))
	{
		try
		{
			// 事件
			ptree eventsNode;
			nodeName = "Events";
			eventsNode = pNode.get_child(nodeName);
			events.deserialize(nodeName, eventsNode);
			return true;
		}
		catch (const std::exception&e)
		{
			console->error("DH {} {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{
			console->error("DH {}", node_name);
			std::exit(1);//节点有错，直接退出
		}
	}
	return false;
}
bool AxisF44Config::deserialize(std::string node_name, ptree pNode)
{
	if (pNode.size() > 0)
	{	
		try
		{
			cnFirst = pNode.get<bool>("<xmlattr>.cnFirst", false);
			camName = pNode.get<std::string>("<xmlattr>.camName");
			ip = pNode.get<std::string>("<xmlattr>.ip");
			nPort = pNode.get<int>("<xmlattr>.nPort");
			lane = pNode.get<int>("<xmlattr>.lane");
			userName = pNode.get<std::string>("<xmlattr>.userName");
			passwd = pNode.get<std::string>("<xmlattr>.passwd");
			usable = lexical_cast<bool>(pNode.get<int>("<xmlattr>.usable"));
			channel = pNode.get<std::string>("<xmlattr>.channel");

			ptree eventsNode;
			nodeName = "Events";
			eventsNode = pNode.get_child(nodeName);
			events.deserialize(nodeName, eventsNode);

			return true;
		}
		catch (const std::exception&e)
		{
			console->error("AxisF44Config: {} {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{

		}
	}
	return false;
}


bool ICEConfig::deserialize(std::string node_name, const ptree pNode)
{
	if (pNode.size() > 0)
	{
		try
		{
			name = pNode.get<std::string>("<xmlattr>.name");
			ip = pNode.get<std::string>("<xmlattr>.ip");
			usable = lexical_cast<bool>(pNode.get<int>("<xmlattr>.usable"));
			lane = pNode.get<int>("<xmlattr>.lane");
			return true;
		}
		catch (const std::exception&e)
		{
			console->error("ICEConfig: {} {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{

		}
	}
	return false;
}


bool EventCond::deserialize(std::string node_name, const ptree pNode)
{
	m_GetCnResult = false;
	m_GetPmnResult = false;
    facets.clear();
    if (pNode.size() > 0)
    {
        ptree attr = pNode.get_child("<xmlattr>");
        for (auto &t : attr)
        {
            std::string attr_name = t.first;
			if (_stricmp(attr_name.c_str(), "Lane") == 0) // 使用车道配置（）
			{
				auto itr = g_LaneCfg.m.find(t.second.data());
				if (itr != g_LaneCfg.m.end())
				{
					facets["trollyMin"] = itr->second.minValue;
					facets["trollyMax"] = itr->second.maxValue;
				}
				else
				{
					int nLaneNum = atoi(t.second.data().c_str());
					bool bFindLane = false;
					for (auto&lane:g_LaneCfg.m)
					{
						if (lane.second.laneNum == nLaneNum)
						{
							bFindLane = true;
							facets["trollyMin"] = lane.second.minValue;
							facets["trollyMax"] = lane.second.maxValue;
							break;
						}
					}
					if (!bFindLane)
					{
						console->error("{} 无效配置参数 {}={}", node_name, attr_name, t.second.data());
					}
				}
			}
			else if (_stricmp(attr_name.c_str(), "trollyMin") == 0)   // 小车编码器下界会覆盖车道配置
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyMax") == 0)   // 小车编码器上界会覆盖车道配置
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyStatus") == 0) // 小车状态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistMin") == 0) // 起升编码器下界
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistMax") == 0) // 起升编码器上界
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistStatus") == 0) // 起升状态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistAction") == 0)   // 起升动作
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyPassValue") == 0) // // 小车经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyPassValueDown") == 0) // // 小车经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyPassValueUp") == 0) // // 小车经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistPassValue") == 0) // 起升经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistPassValueUp") == 0) // 起升上升经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistPassValueDown") == 0) // 起升下降经过某一点
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "lockStatus") == 0) // 开闭锁状态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "lockAction") == 0) // 开闭锁动作
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "lockOccur") == 0) // 开闭锁动作
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "catchStatus") == 0) // 着箱状态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "catchAction") == 0) // 着箱动作
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "spreaderMode") == 0) // 吊具类型
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "spreaderShape") == 0)		//吊具形态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "spreaderAction") == 0) // 吊具类型变化
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistRangeMax") == 0)   // 起升编码器范围最大值
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "hoistRangeMin") == 0)   // // 起升编码器范围最小值
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyRangeMax") == 0)   // 起升编码器范围最大值
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "trollyRangeMin") == 0)   // // 起升编码器范围最小值
			{
				facets["trollyRangeMin"] = lexical_cast<int>(t.second.data());
			}
			//add by wxm 2020.11.25,增加以下6个事件处理
			else if (_stricmp(attr_name.c_str(), "cpsDistanceLess") == 0)	//cps对位距离小于设定值
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "cpsDistancePassValueDown") == 0)   // 对位距离减小经过某值
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "cpsStatus") == 0)		//cps对位状态
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "cpsCnPos") == 0)		//cps压箱位置
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			//add by QB 2021-11-12 理货项目增加条件判断
			else if (_stricmp(attr_name.c_str(), "cpsStatusFinishOccur") == 0) //cps对位完成状态是否发生0=未发生，1=发生
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "cpsStatusLeaveOccur") == 0) //cps对位完成状态是否发生0=未发生，1=发生
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "ContPosOffset") == 0)   // 吊具带箱位置偏移量，用于箱门打开检测
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
		//	else if (_stricmp(attr_name.c_str(), "HoistOffset") == 0)   // 吊具带箱起升偏移量，用于箱门打开检测
		//	{
		//	facets[attr_name] = lexical_cast<int>(t.second.data());
		//	}
			else if (_stricmp(attr_name.c_str(), "hoistToContPassValueDown") == 0)
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "workType") == 0)//任务类型，对应PLC中workType
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "taskAction") == 0)//任务事件，PLC开始需要OCR发送结果，plc中taskid变为非0
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "getCnResult") == 0)//三一请求结果时触发
			{
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "getPmnResult") == 0)//三一请求结果时触发
			{
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "workLane") == 0)//三一请求结果时触发
			{
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if(_stricmp(attr_name.c_str(), "DealWithBackContainerOccur") == 0)
			{ 
				facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "TaskIdChangeOccur") == 0)
			{
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "CnPosBCountPass") == 0)
			{
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "PlcPacketsCounterForTaskPass") == 0)
			{
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else if (_stricmp(attr_name.c_str(), "TruckInLane") == 0)
			{
			console->info("camer TruckInLane read");
			facets[attr_name] = lexical_cast<int>(t.second.data());
			}
			else
			{
				console->error("{} 无效配置参数 {}={}", node_name, attr_name, t.second.data());
			}
        }
        return true;
    }
    return false;
}

bool EventCond::Satisfied(const PLCInfoState& plcInfoState, std::string& cond_info, bool getCnResult, bool getPmnResult) const
{
	if (facets.size() == 0)
	{
		return false;
	}
	bool ret = true;

	auto& plcInfo = plcInfoState.GetCurPLCInfo();

	bool bShowFact = false;
	std::stringstream ssFact;
	//洋浦运行时有崩溃，使用Windbg调试软件发现是ostream申请内存错误，此处添加try-catch QB 22.2.14
	try
	{
		ssFact << "----- " << cond_name << " -----\n";

		for (const auto& t : facets)
		{
			if (ret == false)
			{
				break;
			}

			if (_stricmp(t.first.c_str(), "trollyMin") == 0)
			{
				ret = ret && plcInfo.trollyValue >= t.second;
				ssFact << t.first << ":" << plcInfo.trollyValue << ">=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "trollyMax") == 0)
			{
				ret = ret && plcInfo.trollyValue <= t.second;
				ssFact << t.first << ":" << plcInfo.trollyValue << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistMin") == 0)
			{
				ret = ret && plcInfo.hoistValue >= t.second;
				ssFact << t.first << ":" << plcInfo.hoistValue << ">=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistMax") == 0)
			{
				ret = ret && plcInfo.hoistValue <= t.second;
				ssFact << t.first << ":" << plcInfo.hoistValue << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}

			else if (_stricmp(t.first.c_str(), "lockStatus") == 0)
			{
				ret = ret && plcInfo.lockStatus == t.second;
				ssFact << t.first << ":" << plcInfo.lockStatus << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "catchStatus") == 0)
			{
				ret = ret && plcInfo.catchStatus == t.second;
				ssFact << t.first << ":" << plcInfo.catchStatus << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "spreaderMode") == 0)
			{
				ret = ret && plcInfo.spreaderMode == t.second;
				ssFact << t.first << ":" << plcInfo.spreaderMode << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}

			else if (_stricmp(t.first.c_str(), "trollyPassValue") == 0)
			{
				ret = ret && plcInfoState.isTrollyPassValue(t.second);
				ssFact << t.first << ":" << t.second << " in [" << plcInfoState.GetLastPLC().trollyValue << "," << plcInfoState.GetCurPLCInfo().trollyValue << "]" << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "trollyPassValueUp") == 0)
			{
				ret = ret && plcInfoState.isTrollyPassValueUp(t.second);
				ssFact << t.first << ":" << t.second << " in [" << plcInfoState.GetLastPLC().trollyValue << "," << plcInfoState.GetCurPLCInfo().trollyValue << "]" << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "trollyPassValueDown") == 0)
			{
				ret = ret && plcInfoState.isTrollyPassValueDown(t.second);
				ssFact << t.first << ":" << t.second << " in [" << plcInfoState.GetLastPLC().trollyValue << "," << plcInfoState.GetCurPLCInfo().trollyValue << "]" << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistPassValue") == 0)
			{
				ret = ret && plcInfoState.isHoistPassValue(t.second);
				ssFact << t.first << ":" << t.second << " in [" << plcInfoState.GetLastPLC().hoistValue << "," << plcInfoState.GetCurPLCInfo().hoistValue << "]" << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}

			else if (_stricmp(t.first.c_str(), "hoistPassValueUp") == 0)
			{
				ret = ret && plcInfoState.isHoistPassValueUp(t.second);
				ssFact << t.first << ":" << plcInfoState.GetLastPLC().hoistValue << "<=" << t.second << " && "
					<< plcInfoState.GetCurPLCInfo().hoistValue << ">" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistPassValueDown") == 0)
			{
				ret = ret && plcInfoState.isHoistPassValueDown(t.second);
				ssFact << t.first << ":" << plcInfoState.GetLastPLC().hoistValue << ">=" << t.second << " && "
					<< plcInfoState.GetCurPLCInfo().hoistValue << "<" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "lockAction") == 0)
			{
				if (t.second == 1)
				{
					ret = ret && plcInfoState.isLockOccur();
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().lockStatus << "==" << PLCInfo::UNLOCKED << " && "
						<< plcInfoState.GetCurPLCInfo().lockStatus << "==" << PLCInfo::LOCKED << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2)
				{
					ret = ret && plcInfoState.isUnLockOccur();
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().lockStatus << "==" << PLCInfo::LOCKED << " && "
						<< plcInfoState.GetCurPLCInfo().lockStatus << "==" << PLCInfo::UNLOCKED << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = false;
				}
			}
			else if (_stricmp(t.first.c_str(), "lockOccur") == 0)
			{
				if (t.second == 1) // 发生开闭锁
				{
					ret = ret && (plcInfoState.isLockOccur() || plcInfoState.isUnLockOccur());
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().lockStatus << "," << plcInfoState.GetCurPLCInfo().lockStatus << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2) // 未发生开闭锁
				{
					ret = ret && (!plcInfoState.isLockOccur() && !plcInfoState.isUnLockOccur());
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().lockStatus << "," << plcInfoState.GetCurPLCInfo().lockStatus << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = false;
				}
			}
			else if (_stricmp(t.first.c_str(), "catchAction") == 0)
			{
				if (t.second == 1) // 发生着箱
				{
					ret = ret && plcInfoState.isCatchOccur();
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().catchStatus << "==" << PLCInfo::NON_CATCHING << " && "
						<< plcInfoState.GetCurPLCInfo().catchStatus << "==" << PLCInfo::CATCHING << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2) // 着箱信号消失
				{
					ret = ret && plcInfoState.isUnCatchOccur();
					ssFact << t.first << ":" << plcInfoState.GetLastPLC().catchStatus << "==" << PLCInfo::CATCHING << " && "
						<< plcInfoState.GetCurPLCInfo().catchStatus << "==" << PLCInfo::NON_CATCHING << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = false;
				}
			}
			else if (_stricmp(t.first.c_str(), "spreaderAction") == 0)
			{
				if (t.second == 1)    // 变为短箱
				{
					ret = ret && plcInfoState.isSpreaderMode() && plcInfo.spreaderMode == PLCInfo::SINGLE20;
					ssFact << t.first << "变为短箱:" << plcInfoState.GetLastPLC().spreaderMode << " -> " << plcInfoState.GetCurPLCInfo().spreaderMode << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2) // 变为长箱
				{
					ret = ret && plcInfoState.isSpreaderMode() && plcInfo.spreaderMode == PLCInfo::SINGLE40;
					ssFact << t.first << "变为长箱:" << plcInfoState.GetLastPLC().spreaderMode << " -> " << plcInfoState.GetCurPLCInfo().spreaderMode << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 3) // 变为双箱
				{
					ret = ret && plcInfoState.isSpreaderMode() && plcInfo.spreaderMode == PLCInfo::DOUBLE;
					ssFact << t.first << "变为双箱:" << plcInfoState.GetLastPLC().spreaderMode << " -> " << plcInfoState.GetCurPLCInfo().spreaderMode << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = false;
				}
			}
			else if (_stricmp(t.first.c_str(), "hoistRangeMax") == 0)
			{
				ret = ret && plcInfo.hoistValue <= t.second;
				ssFact << t.first << ":" << plcInfo.hoistValue << ">=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistRangeMin") == 0)
			{
				ret = ret && plcInfo.hoistValue >= t.second;
				ssFact << t.first << ":" << plcInfo.hoistValue << ">=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "trollyRangeMax") == 0)
			{
				ret = ret && plcInfo.trollyValue <= t.second;
				ssFact << t.first << ":" << plcInfo.trollyValue << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "trollyRangeMin") == 0)
			{
				ret = ret && plcInfo.trollyValue >= t.second;
				ssFact << t.first << ":" << plcInfo.trollyValue << ">=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			//wxm 2020.11.25 增加以下6个条件符合性判断
			else if (_stricmp(t.first.c_str(), "cpsDistanceLess") == 0)
			{
				ret = ret && abs(plcInfo.cpsDistance) <= t.second;
				ssFact << t.first << ":" << abs(plcInfo.cpsDistance) << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "cpsStatus") == 0)
			{
				ret = ret && plcInfo.cpsStatus == t.second;
				ssFact << t.first << ":" << plcInfo.cpsStatus << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "cpsCnPos") == 0)
			{
				ret = ret && plcInfo.cpsCnPos == t.second;				//压箱位置
				ssFact << t.first << ":" << plcInfo.cpsCnPos << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "cpsStatusFinishOccur") == 0)
			{
				ret = ret && plcInfoState.IsCpsStatusFinishOccur();
				ssFact << t.first << ":" << plcInfoState.GetLastPLC().cpsStatus << "!=" << PLCInfo::SEA_CPS_FINISH
					<< " && " << plcInfoState.GetCurPLCInfo().cpsStatus << "==" << PLCInfo::SEA_CPS_FINISH << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}

			else if (_stricmp(t.first.c_str(), "cpsDistancePassValueDown") == 0)
			{
				ret = ret && plcInfoState.IsCpsDistanceDownPassValue(t.second);
				ssFact << t.first << ":" << plcInfoState.GetLastPLC().cpsDistance << ">=" << t.second
					<< " && " << plcInfoState.GetCurPLCInfo().cpsDistance << "<" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "hoistAction") == 0)
			{
				if (t.second == 1)
				{
					ret = ret && plcInfoState.IsHoistUpOccur();
					ssFact << t.first << ":" << "HoistUpOccur" << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2)
				{
					ret = ret && plcInfoState.IsHoistDownOccur();
					ssFact << t.first << ":" << "HoistDownOccur" << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = ret && false;
				}
			}
			else if (_stricmp(t.first.c_str(), "ContPosOffset") == 0)
			{
				bool bRet = false;
				string strInfo;
				if (plcInfoState.isHoistPassValue(g_nLockHoistValue - t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
				{
					g_nLockHoistValue -= t.second;
					bRet = true;
					strInfo += fmt::format("[HoistPass {}]", g_nLockHoistValue);
				}
				if (plcInfoState.isHoistPassValue(g_nLockHoistValue + t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
				{
					g_nLockHoistValue += t.second;
					bRet = true;
					strInfo += fmt::format("[HoistPass {}]", g_nLockHoistValue);
				}
				if (plcInfoState.isTrollyPassValue(g_nLockTrallyValue - t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
				{
					g_nLockTrallyValue -= t.second;
					bRet = true;
					strInfo += fmt::format("[TrollyPass {}]", g_nLockTrallyValue);
				}
				if (plcInfoState.isTrollyPassValue(g_nLockTrallyValue + t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
				{
					g_nLockTrallyValue += t.second;
					bRet = true;
					strInfo += fmt::format("[TrollyPass {}]", g_nLockTrallyValue);
				}
				ret = ret && bRet;
				ssFact << t.first << ":" << strInfo << (ret ? "=true\n" : "=false\n");
				//bShowFact |= ret;
				bShowFact = true;
			}
			/*else if (_stricmp(t.first.c_str(), "HoistOffset") == 0)
			{
			bool bRet = false;
			string strInfo;
			if (plcInfoState.isHoistPassValue(g_nLockHoistValue1 - t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
			{
				g_nLockHoistValue1 -= t.second;
				bRet = true;
				strInfo += fmt::format("[HoistPass {}]", g_nLockHoistValue1);
			}
			if (plcInfoState.isHoistPassValue(g_nLockHoistValue1 + t.second) && plcInfo.lockStatus == PLCInfo::LOCKED)
			{
				g_nLockHoistValue1 += t.second;
				bRet = true;
				strInfo += fmt::format("[HoistPass {}]", g_nLockHoistValue1);
			}
			ret = ret && bRet;
			ssFact << t.first << ":" << strInfo << (ret ? "=true\n" : "=false\n");
			bShowFact |= ret;
			}*/
			else if (_stricmp(t.first.c_str(), "hoistToContPassValueDown") == 0)
			{
				ret = ret && plcInfoState.IsHoistToContPassValueDown(t.second);
				ssFact << t.first << ":" << plcInfoState.GetLastPLC().hoistToCont << ">=" << t.second
					<< " && " << plcInfoState.GetCurPLCInfo().hoistToCont << "<" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}

			else if (_stricmp(t.first.c_str(), "taskAction") == 0)
			{
				if (t.second == 1)
				{
					ret = ret && plcInfoState.IsStartSendResultToPlc();
					ssFact << t.first << ":StartSendResult " << plcInfoState.GetLastPLC().taskId << "->" << plcInfoState.GetCurPLCInfo().taskId << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else
				{
					ret = ret && plcInfoState.IsStopSendResultToPlc();
					ssFact << t.first << ":StopSendResult " << plcInfoState.GetLastPLC().taskId << "->" << plcInfoState.GetCurPLCInfo().taskId << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
			}
			else if (_stricmp(t.first.c_str(), "workType") == 0)
			{
				ret = ret && plcInfo.workType == t.second;
				ssFact << t.first << ":" << plcInfo.workType << "==" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "getCnResult") == 0)
			{
				ret = ret && getCnResult;
				ssFact << t.first << ":" << getCnResult;
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "getPmnResult") == 0)
			{
				ret = ret && getPmnResult;
				ssFact << t.first << ":" << getPmnResult;
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "workLane") == 0)
			{
				ret = ret && (plcInfo.workLine == t.second);
				ssFact << t.first << ":" << "plc workLane:" << plcInfo.workLine << "  set:" << t.second;

				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "DealWithBackContainerOccur") == 0)
			{
				ret = ret && plcInfoState.DealWithBackContainerOccur();
				ssFact << t.first << ":" << plcInfoState.GetCurPLCInfo().cpsCnPos << "==" << 3
					<< " && " << plcInfoState.GetLastPLC().cpsCnPos << "!=" << 3 << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "TaskIdChangeOccur") == 0)
			{
				ret = ret && plcInfoState.TaskIdChangeOccur();
				ssFact << t.first << ":" << plcInfoState.GetCurPLCInfo().taskId << "==" <<
					" && " << plcInfoState.GetLastPLC().taskId << "!=" << 3 << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "CnPosBCountPass") == 0)
			{
				ret = ret && plcInfoState.CnPosBCountPass(t.second);
				ssFact << t.first << ":" << plcInfoState.cnPosBcountCur << ">" << t.second <<
					" && " << plcInfoState.cnPosBcountOld << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "PlcPacketsCounterForTaskPass") == 0)
			{
				ret = ret && plcInfoState.PlcPacketsCounterForTaskPass(t.second);
				ssFact << t.first << ":" << plcInfoState.plcPacketsCounterForTask_cur << ">" << t.second <<
					" && " << plcInfoState.plcPacketsCounterForTask_old << "<=" << t.second << (ret ? "=true\n" : "=false\n");
				bShowFact |= ret;
			}
			else if (_stricmp(t.first.c_str(), "TruckInLane") == 0)
			{
				if (t.second == 1)
				{
					ret = ret && plcInfoState.IsTruckInLane1();
					ssFact << t.first << ":" << plcInfoState.GetCurPLCInfo().hasTruck[1] << "==" << 1 << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
				else if (t.second == 2)
				{
					ret = ret && plcInfoState.IsTruckInLane2();
					ssFact << t.first << ":" << plcInfoState.GetCurPLCInfo().hasTruck[2] << "==" << 1 << (ret ? "=true\n" : "=false\n");
					bShowFact |= ret;
				}
			}
			if (bShowFact)
			{
				//ssFact << "gantryV:" << std::to_string(plcInfo.gantryValue) << "   trollyV:" << std::to_string(plcInfo.trollyValue) << "     hoistV:" << std::to_string(plcInfo.hoistValue) << ";\n";
				cond_info = ssFact.str();
			}
		}
	}
		catch (std::exception& e)
		{
			console->error("EventCond::Satisfied ssFact:{}  error:{}", ssFact.str(), e.what());
		}
		catch (...)
		{
			console->error("EventCond::Satisfied ssFact:{} error", ssFact.str());
		}
		return ret;

	}

bool Event::deserialize(std::string node_name, const ptree pNode)
{
    if (pNode.size() > 0)
    {
		// 事件类型
		try
		{
			evtType = pNode.get<std::string>("<xmlattr>.evtType");
		}
		catch (const std::exception&e)
		{
			console->error("evtType error {}", e.what());
			std::exit(1);
		}
		catch (...)
		{
			console->error("evtType error");
			std::exit(1);
		}

		evtName = node_name;
		//是否发送结果
		sendResult = pNode.get<bool>("<xmlattr>.sendResult", false);
		console->info("Read {}", node_name);//add GUN Y01_CAP_CON info

		bool bIsTrigger = false;
		if (evtType == "SET") // 设置事件
		{
			try
			{
				ptz.P = pNode.get<int>("<xmlattr>.P");
				ptz.T = pNode.get<int>("<xmlattr>.T");
				ptz.Z = pNode.get<int>("<xmlattr>.Z");
				//bDECODE = true;
				autoCap = pNode.get<std::string>("<xmlattr>.AUTO_CAP","");
			}
			catch (const std::exception&e)
			{
				console->error("PTZ error {}",e.what());
				std::exit(1);
			}
			catch (...)
			{

			}
			
		}
		else if (evtType == "CAP")
		{
			/*
			<RecogType>
				<CN type = "1" roi = "0,0,0,0" rotate = "" reverse = "0" rectify=""/>
				<CN type = "1" roi = "" rotate = "" reverse = "0"  rectify=""/>
				<DOOR type = "4" roi = "" rotate = "" reverse = "0"  rectify=""/>
				< / RecogType>
			*/
			auto childs = pNode.get_child("");
			for (auto&child : childs)
			{
				if (child.first == "RecogType")
				{
					if (child.second.size() > 0)
					{
						auto attr = child.second.get_child("");
						for (auto &t : attr)
						{
							if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
							{
								RecogParam param;
								param.name = t.first;
								auto str = t.second.get<std::string>("<xmlattr>.rtype");
								param.truckType = t.second.get<int>("<xmlattr>.truckType", 0);
								if (str.length())
									param.rtype = (ocrs::pb::GRpc_RecogParam::G_RType)atoi(str.c_str());

								param.rectify = t.second.get<std::string>("<xmlattr>.rectify", "");
								param.reverse = t.second.get<bool>("<xmlattr>.reverse", false);
								param.rotate = t.second.get<int>("<xmlattr>.rotate", 0);

								str = t.second.get<std::string>("<xmlattr>.roi");
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
										console->error("Roi error");
										std::exit(1);//数据格式错误，程序直接退出
									}
								}
								//--end
								param.roi_str = str;
								arrRecogParam.push_back(param);
							}
						}
					}
				}
			}
		}
		else if (evtType == "STEP")
		{
			/*
			<EventStep>
				<Step1 evt_name="Y1_SET_LANE64_SINGLE"/>
			</EventStep>
			*/
			auto childs = pNode.get_child("");
			for (auto&child : childs)
			{
				if (child.first == "StepEvent")
				{
					if (child.second.size() > 0)
					{
						auto attr = child.second.get_child("");
						for (auto &t : attr)
						{
							if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
							{
								std::string evt_name = t.second.get<std::string>("<xmlattr>.evt_name");
								arrStepEvent.push_back(evt_name);
							}
						}
					}
				}
			}
		}
		else if (evtType == "TRIGGER")
		{
			bIsTrigger = true;
			/*
			<EventStep>
				<Step1 evt_name="Y1_SET_LANE64_SINGLE"/>
			</EventStep>
			*/
			ptree attr = pNode.get_child("<xmlattr>");
			for (auto &t : attr)
			{
				if (t.first == "spreader")
				{
					spreader = t.second.data();
				}
				else if (t.first == "truck_dir")
				{
					truck_dir = t.second.data();
				}
				else if (t.first == "cont_pos")
				{
					cont_pos = t.second.data();
				}
			}
			auto childs = pNode.get_child("");
			for (auto&child : childs)
			{
				if (child.first == "StepEvent")
				{
					if (child.second.size() > 0)
					{
						auto attr = child.second.get_child("");
						for (auto &t : attr)
						{
							if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
							{
								std::string evt_name = t.second.get<std::string>("<xmlattr>.evt_name");
								arrStepEvent.push_back(evt_name);
							}
						}
					}
				}
			}
		}

		if (bIsTrigger)
		{
			//TRIGGER事件不加载充分和必要条件
			return true;
		}
		// 充分条件（多个）
		auto sufficientCondNode = pNode.get_child("SufficientCond");
		if (sufficientCondNode.size() > 0)
		{
			sufficientConds.clear();
			auto attr = sufficientCondNode.get_child("");
			for (auto &t : attr)
			{
				if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
				{
					EventCond cond;
					if (cond.deserialize(node_name + " SufficientCond", t.second))
					{
						cond.cond_name = t.first;
						sufficientConds[t.first] = cond;
					}
				}
			}
		}
		// 必要条件（唯一）
		auto necessaryCondNode = pNode.get_child("NecessaryCond");
		// 如果存在充分条件或者必要条件
		necessaryCond.deserialize(node_name + " NecessaryCond", necessaryCondNode);
		return true;
    }
    return false;
}

bool Event::Triggered(const PLCInfoState& plcInfoState, std::string event_name, bool getCnResult, bool getPmnResult) const
{
    // 全部条件满发即可
	std::string cond_necessary, cond_sufficient;
	if (necessaryCond.Satisfied(plcInfoState, cond_necessary,getCnResult, getPmnResult))
	{
		if (sufficientConds.empty())
		{
			console->info("========TRIGGER========NecessaryCond:{} {}", event_name, cond_necessary);
			return true;
		}
		for (auto& t : sufficientConds)
		{
			if (t.second.Satisfied(plcInfoState, cond_sufficient, getCnResult, getPmnResult))
			{
				// 只要一个条件满发即可
				console->warn("========TRIGGER========NecessaryCond:{} {} SufficientCond:{}", event_name, cond_necessary, cond_sufficient);
				return true;
			}
			if (!cond_sufficient.empty())
				console->info("NecessaryCond:{} {} SufficientCond:{}", event_name, cond_necessary, cond_sufficient);
		}
    }
	/*if ()
	{
		
	}*/
	//console->info("========TRIGGER========NecessaryCond:{} {}", event_name, cond_necessary);
    return false;
}

bool EventsConfig::deserialize(std::string node_name, const ptree pNode)
{
    eventList.clear();
    if (pNode.size() > 0)
    {
        for (auto &t : pNode)
        {
            if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
            {
                ptree childNode;
                std::string nodeName = t.first;
                childNode = pNode.get_child(nodeName);
                // 跳过无效的事件
                Event evt;
                if (evt.deserialize(nodeName, childNode))
                {
                    std::string name = nodeName;
                    eventList[name] = evt;
                }
            }
        }
        return true;
    }
    return false;
}

bool PositionConfig::deserialize(std::string node_name, const ptree pNode)
{
	if (pNode.size() > 0)
	{
		for (auto &t : pNode)
		{
			if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
			{
				ptree attr = t.second.get_child("<xmlattr>");
				for (auto &tt : attr)
				{
					std::string a = tt.first;
					int b = atoi(tt.second.data().c_str());
					m.insert(make_pair(a, b));
				}
			}
		}
	}
	return true;
}
//
bool LaneConfig::deserialize(std::string node_name, const ptree pNode)
{
    if (pNode.size() > 0)
    {
		try
		{
			for (auto &t : pNode)
			{
				if (t.first != "<xmlattr>" && "<xmlcomment>" != t.first)
				{
					console->info("read {}", t.first);
					Range laneRange;
					laneRange.maxValue = t.second.get<int>("<xmlattr>.trollyMax");
					laneRange.minValue = t.second.get<int>("<xmlattr>.trollyMin");
					laneRange.laneNum = t.second.get<int>("<xmlattr>.LaneNum");
					std::string key = t.first;
					m[key] = laneRange;
				}
			}
			return true;
		}
		catch (const std::exception&e)
		{
			console->error("load {} error {}", node_name, e.what());
			std::exit(1);//节点有错，直接退出
		}
		catch (...)
		{
			console->error("load {} error", node_name);
			std::exit(1);//节点有错，直接退出
		}
    }
    return false;
}
