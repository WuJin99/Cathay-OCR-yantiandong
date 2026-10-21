#include "AppConfig.h"
#include "Console.h"

AppConfig g_AppConfig;

using namespace std;

void ParseBrokerCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		g_AppConfig.brokerCfg.IP = pNode.get<std::string>("<xmlattr>.ipaddr");
		g_AppConfig.brokerCfg.xpub_port = pNode.get<int>("<xmlattr>.xpub_port");
		g_AppConfig.brokerCfg.sub_port = pNode.get<int>("<xmlattr>.sub_port");
		g_AppConfig.brokerCfg.resend_count = pNode.get<int>("<xmlattr>.resend_count");
	}
}

// user 配置信息配置
void ParseUserCfgNode(const ptree pNode)
{
    if (pNode.size() > 0)
    {

		g_AppConfig.userCfg.imgRootPath = pNode.get<std::string>("<xmlattr>.imgRootPath");
		g_AppConfig.userCfg.pmnSimilarMatch = pNode.get<bool>("<xmlattr>.pmnSimilarMatch", 1);
		g_AppConfig.userCfg.quayNo = pNode.get<std::string>("<xmlattr>.quayNo");
		g_AppConfig.userCfg.IARSName = pNode.get<std::string>("<xmlattr>.IARSName");
		g_AppConfig.userCfg.sendInfoShow = pNode.get<int>("<xmlattr>.sendInfoShow",0);
		g_AppConfig.userCfg.localIp = pNode.get<std::string>("<xmlattr>.localIp");
		g_AppConfig.userCfg.savePath = pNode.get<std::string>("<xmlattr>.savePath");
		g_AppConfig.userCfg.needReplyResend = pNode.get<int>("<xmlattr>.needReplyResend", 60);
		g_AppConfig.userCfg.stateInfoInterval = pNode.get<int>("<xmlattr>.stateInfoInterval", 60);
		g_AppConfig.userCfg.heartBeatInterval = pNode.get<int>("<xmlattr>.heartBeatInterval", 60);
		g_AppConfig.userCfg.heartBeatTimeout = pNode.get<int>("<xmlattr>.heartBeatTimeout", 60);
		g_AppConfig.userCfg.wait_ptz_sec = pNode.get<int>("<xmlattr>.wait_ptz_sec", 3);
		g_AppConfig.userCfg.gantryChangeBayValue = pNode.get<int>("<xmlattr>.gantryChangeBayValue", 300);
		g_AppConfig.userCfg.sdk_log = pNode.get<bool>("<xmlattr>.sdk_log", 0);
		g_AppConfig.userCfg.only_pmn = pNode.get<bool>("<xmlattr>.only_pmn", 0);
		g_AppConfig.userCfg.ecs_ms = pNode.get<int>("<xmlattr>.ecs_ms", 4000);
		g_AppConfig.userCfg.door_open_conf = pNode.get<float>("<xmlattr>.door_open_conf", 0.6);
		g_AppConfig.userCfg.door_open_count = pNode.get<int>("<xmlattr>.door_open_count", 2);
		g_AppConfig.userCfg.reset_dooropen_by_hoist_value = pNode.get<int>("<xmlattr>.reset_dooropen_by_hoist_value", 1500);
		g_AppConfig.userCfg.sendTruckInterval = pNode.get<int>("<xmlattr>.sendTruckInterval", 500);
		g_AppConfig.userCfg.sendTruckTimeOut = pNode.get<int>("<xmlattr>.sendTruckTimeOut", 2000);
		g_AppConfig.userCfg.hoistToContUpVal = pNode.get<int>("<xmlattr>.hoistToContUpVal", 200);
		g_AppConfig.userCfg.pmn_ganty = pNode.get<int>("<xmlattr>.pmn_ganty", 400);
		g_AppConfig.userCfg.CRS_GRPC_WaitTime = pNode.get<int>("<xmlattr>.CRS_GRPC_WaitTime", 4);

		// 入池记样本；拉号时按 PIIS 请求瞬间的 gantry 窗口聚合
		g_AppConfig.userCfg.pmnWindowEnable = pNode.get<bool>("<xmlattr>.pmnWindowEnable", 1);
		// 窗口半径，默认约一辆集卡长度 13m（编码器×100）
		g_AppConfig.userCfg.pmnWindowGantryDelta = pNode.get<int>("<xmlattr>.pmnWindowGantryDelta", 1300);
		// hasTruck 无车消抖帧数，达阈值才 ClearPmnCache（清车顶号+车牌），默认为3
		g_AppConfig.userCfg.pmnNoTruckClearFrames = pNode.get<int>("<xmlattr>.pmnNoTruckClearFrames", 3);
    }
}
// 船公司映射列表配置
void ParseOwnerNumberCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		for (auto &t : pNode)
		{
			if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
			{
				std::string s = t.second.get<std::string>("<xmlattr>.src");
				std::string d = t.second.get<std::string>("<xmlattr>.dst");
				g_AppConfig.mapOwnerNum[s] = d;
			}
		}
	}
}
void ParseTruckNumberCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		for (auto &t : pNode)
		{
			if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
			{
				TruckNumHold hold;
				char src = t.second.get<char>("<xmlattr>.src");
				char dst = t.second.get<char>("<xmlattr>.dst");
				hold.src = src;
				hold.dst = dst;
				g_vecTruckNum.push_back(hold);
			}
		}
	}
}
// ISO映射列表配置
void ParseIsoNumberCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		for (auto &t : pNode)
		{
			if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
			{
				std::string s = t.second.get<std::string>("<xmlattr>.src");
				std::string d = t.second.get<std::string>("<xmlattr>.dst");
				g_AppConfig.mapIsoNum[s] = d;
			}
		}
	}
}

// FTP配置
void ParseFtpCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		g_AppConfig.ftpCfg.enable = lexical_cast<bool>(pNode.get<int>("<xmlattr>.enable"));
		g_AppConfig.ftpCfg.ftpHost = pNode.get<std::string>("<xmlattr>.host");
		g_AppConfig.ftpCfg.ftpPort = pNode.get<std::string>("<xmlattr>.port");
		g_AppConfig.ftpCfg.ftpUser = pNode.get<std::string>("<xmlattr>.usr");
		g_AppConfig.ftpCfg.ftpPwd = pNode.get<std::string>("<xmlattr>.password");
		g_AppConfig.ftpCfg.reSaveCounts = pNode.get<int>("<xmlattr>.reSaveCounts",20);
	}
}
// GRPC配置
void ParseCrsCfgNode(const ptree& pNode)
{
	if (pNode.size() > 0)
	{
		ptree childs = pNode.get_child("");
		for (auto &t : childs)
		{
			if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
			{
				if (t.second.size() > 0)
				{
					int enable = t.second.get<int>("<xmlattr>.enable", 0);
					if (enable)
					{
						CrsConfig cc;
						cc.crs_name = t.first;
						cc.ipaddr = t.second.get<std::string>("<xmlattr>.ipaddr");
						cc.port = t.second.get<std::string>("<xmlattr>.port");
						g_AppConfig.arrCrsCfg.push_back(cc);
					}
				}
			}
		}
	}
}
// Monitor配置
void ParseMonitorCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		g_AppConfig.monitorCfg.CPUCfg = pNode.get<int>("<xmlattr>.cpu_rate", 90);
		g_AppConfig.monitorCfg.GPUCfg = pNode.get<int>("<xmlattr>.gpu_rate", 98);
		g_AppConfig.monitorCfg.diskCfg = pNode.get<int>("<xmlattr>.diskfree_rate", 10);
		if (g_AppConfig.monitorCfg.diskCfg < 2 || g_AppConfig.monitorCfg.diskCfg > 50)
		{
			console->warn("diskfree_rate {} 超出范围[2,50]，默认10", g_AppConfig.monitorCfg.diskCfg);
			g_AppConfig.monitorCfg.diskCfg = 10;
		}
		g_AppConfig.monitorCfg.memCfg = pNode.get<int>("<xmlattr>.memory_rate", 90);
	}
}

//检查是否为无BOM的UTF8
bool check_utf8_without_bom(const string &file_name)
{
	std::ifstream file_in;
	file_in.open(file_name, ios::in);

	if (!file_in.is_open())
	{
		cout << "打开文件失败" << endl;
		return false;
	}

	stringstream buffer;
	buffer << file_in.rdbuf();
	file_in.close();
	string text = buffer.str();

	size_t len = text.size();
	int n = 0;
	unsigned char ch;
	bool b_all_ascii = true;

	//0x00-0x7F为ASCII码范围

	for (size_t i = 0; i < len; ++i)
	{
		ch = text[i];

		if ((ch & 0x80) != 0)
		{
			b_all_ascii = false;
		}

		if (n == 0)
		{
			if (ch >= 0x80)
			{
				if (ch >= 0xFC && ch <= 0xFD)
				{
					n = 6;
				}
				else if (ch >= 0xF8)
				{
					n = 5;
				}
				else if (ch >= 0xF0)
				{
					n = 4;
				}
				else if (ch >= 0xE0)
				{
					n = 3;
				}
				else if (ch >= 0xC0)
				{
					n = 2;
				}
				else
				{
					return false;
				}
				n--;
			}
		}
		else
		{
			if ((ch & 0xC0) != 0x80)//在UTF-8中，以位模式10开始的所有字节是多字节序列的后续字节
			{
				return false;
			}
			n--;
		}
	}

	if (n > 0)
	{
		return false;
	}

	if (b_all_ascii)
	{
		return false;
	}

	return true;
}

bool IsUtf8(const char*pszPath)
{
	std::ifstream fin(pszPath, ios::binary);
	unsigned char  s2;
	fin.read((char*)&s2, sizeof(s2));//读取第一个字节，然后左移8位
	unsigned short p = s2 << 8;
	fin.read((char*)&s2, sizeof(s2));//读取第二个字节
	p |= s2;
	fin.close();

	string code;
	switch (p)//判断文本前两个字节
	{
	case 0xfffe:  //
		code = "Unicode";
		break;
	case 0xfeff://
		code = "Unicode big endian";
		break;
	case 0xefbb://
	{
		code = "UTF-8";
		return  true;
	}
	default:
		if (check_utf8_without_bom(pszPath))
		{
			code = "UTF-8";
			return  true;
		}
		else
			code = "ANSI";
	}
	return false;
}

string Utf8ToAnsi(const char*pszUtf8)
{
	int nLen = MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, 0, 0);
	wchar_t wch[100] = {};
	MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, wch, nLen);
	nLen = WideCharToMultiByte(936, 0, wch, -1, nullptr, 0, 0, 0);
	char ch[100] = {};
	WideCharToMultiByte(936, 0, wch, -1, ch, nLen, 0, 0);
	return ch;
}
// PLC 服务配置信息
void ParsePLCHttpCfgNode(PLCHttpConfig& plcHttpCfg, const ptree pNode)
{
	if (pNode.size() > 0)
	{
		plcHttpCfg.plcPath = pNode.get<std::string>("<xmlattr>.plcPath","");
		plcHttpCfg.localHostIP = pNode.get<std::string>("<xmlattr>.localHostIP", "");
		plcHttpCfg.waitTime = pNode.get<int>("<xmlattr>.waitTime", 0);
		plcHttpCfg.serverPort = pNode.get<int>("<xmlattr>.serverPort", 0);
		plcHttpCfg.doorDirPath = pNode.get<std::string>("<xmlattr>.doorDirPath", "");
		plcHttpCfg.doorOpenPath = pNode.get<std::string>("<xmlattr>.doorOpenPath", "");
		plcHttpCfg.cnDataPath = pNode.get<std::string>("<xmlattr>.cnDataPath", "");
		plcHttpCfg.pmnDataPath = pNode.get<std::string>("<xmlattr>.pmnDataPath", "");
		plcHttpCfg.remoteIP = pNode.get<std::string>("<xmlattr>.remoteIP", "127.0.0.1");
		plcHttpCfg.remotePort = pNode.get<int>("<xmlattr>.remotePort",0);

	}
}
void ParseSubCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		ptree childs = pNode.get_child("");
		for (auto &t : childs)
		{
			if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
			{
				if (t.second.size() > 0)
				{
					int enable = t.second.get<int>("<xmlattr>.enable", 0);
					if (enable)
					{
						SubConfig sc;
						sc.ipaddr = t.second.get<std::string>("<xmlattr>.ipaddr");
						sc.port = t.second.get<int>("<xmlattr>.port");
						if (sc.port > 0)
							g_AppConfig.arrSubCfg.push_back(sc);
					}
				}
			}
		}
	}
}
// PLC 配置信息
void ParsePLCCfgNode(const ptree pNode)
{
	if (pNode.size() > 0)
	{
		ptree attr = pNode.get_child("<xmlattr>");

		for (auto &t : attr)
		{
			if (t.first == "commType")
			{
				g_AppConfig.plcCfg.commType = lexical_cast<int>(t.second.data());
			}
			else if (t.first == "slaveAddr")
			{
				g_AppConfig.plcCfg.slaveAddr = lexical_cast<int>(t.second.data());
			}
			else if (t.first == "readAddr")
			{
				g_AppConfig.plcCfg.readAddr = lexical_cast<int>(t.second.data());
			}
			else if (t.first == "writeAddr")
			{
				g_AppConfig.plcCfg.writeAddr = lexical_cast<int>(t.second.data());
			}
			else if (t.first == "rrTimeout")
			{
				g_AppConfig.plcCfg.rrTimeout = lexical_cast<int>(t.second.data());
			}
			else if (t.first == "rrWaitTime")
			{
				g_AppConfig.plcCfg.rrWaitTime = lexical_cast<int>(t.second.data());
			}
		}

		// PLC UDP
		auto httpPlc = pNode.get_child("Http");
		ParsePLCHttpCfgNode(g_AppConfig.plcCfg.plcHttpCfg, httpPlc);
	}
}

void LoadAppConfigXML(const std::string xmlPath)
{
	try
	{
		ptree xmlRootNode;
		read_xml(xmlPath, xmlRootNode);
		auto child = xmlRootNode.get_child("GalaxyVism");

		for (auto pNode = child.begin(); pNode != child.end(); ++pNode)
		{
			// 节点信息
			if (pNode->first == "PLC")
			{
				console->info("解析 {}", pNode->first);
				ParsePLCCfgNode(pNode->second);
			}
			// 节点信息
			else if (pNode->first == "BROKER")
			{
				console->info("Load {} ", pNode->first);
				ParseBrokerCfgNode(pNode->second);
			}
			else if (pNode->first == "USER")
			{
				console->info("Load {} ", pNode->first);
				ParseUserCfgNode(pNode->second);
			}
			else if (pNode->first == "OWNERNUMBER")
			{
				console->info("Load {} ", pNode->first);
				ParseOwnerNumberCfgNode(pNode->second);
			}
			else if (pNode->first == "ISONUMBER")
			{
				console->info("Load {} ", pNode->first);
				ParseIsoNumberCfgNode(pNode->second);
			}
			else if (pNode->first == "FTP")
			{
				console->info("Load {} ", pNode->first);
				ParseFtpCfgNode(pNode->second);
			}
			else if (pNode->first == "CRS")
			{
				console->info("Load {} ", pNode->first);
				ParseCrsCfgNode(pNode->second);
				if (g_AppConfig.arrCrsCfg.size() <= 0)
				{
					console->error("无有效CRS配置，请检查");
					//exit(1);
				}
			}
			else if (pNode->first == "MONITOR")
			{
				console->info("Load {} ", pNode->first);
				ParseMonitorCfgNode(pNode->second);
			}
			else if (pNode->first == "LANESIDE")
			{
				console->info("Load {} ", pNode->first);
				ptree childs = pNode->second.get_child("");
				for (auto &t : childs)
				{
					if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
					{
						if (t.second.size() > 0)
						{
							LaneSide side;
							side.name = t.second.get<std::string>("<xmlattr>.name");
							side.lane_id = t.second.get<std::string>("<xmlattr>.lane_id");
							side.cfg = t.second.get<std::string>("<xmlattr>.cfg");
							g_AppConfig.arrLaneSide.push_back(side);
						}
					}
				}
				if (g_AppConfig.arrLaneSide.size() <= 0)
				{
					console->error("无有效LANESIDE配置，请检查");
					exit(1);
				}
			}
			else if (pNode->first == "SUB")
			{
				console->info("Load {} ", pNode->first);
				ParseSubCfgNode(pNode->second);
			}
			else if (pNode->first == "TRUCKNUM")
			{
				console->info("Load {} ", pNode->first);
				ParseTruckNumberCfgNode(pNode->second);
			}
			/*else if (pNode->first == "PMNDIGIT")
			{
				console->info("Load {} ", pNode->first);
				ptree childs = pNode->second.get_child("");
				for (auto &t : childs)
				{
					if (t.first != "<xmlattr>" && t.first != "<xmlcomment>")
					{
						if (t.second.size() > 0)
						{
							string Pmn;
							Pmn = t.second.get<std::string>("<xmlattr>.pmn");
							g_AppConfig.m_PmnComp.push_back(Pmn);
						}
					}
				}
			}*/
		}
	}
	catch (std::exception& e)
	{
		console->error("LoadAppConfig {} fail:{}", xmlPath, e.what());
		exit(1);
	}
	catch (...)
	{
		console->error("LoadAppConfig {} fail", xmlPath);
		exit(1);
	}

	//从txt中读取需要屏蔽的错误车牌识别结果
	bool bIsUtf8 = IsUtf8("invalid_plate.txt");
	std::ifstream ifs("invalid_plate.txt");
	if (ifs.is_open())
	{
		while (!ifs.eof())
		{
			char szInvalidPlate[100] = {};
			ifs.getline(szInvalidPlate, 100);
			if (strlen(szInvalidPlate) > 0)
			{
				string plate = szInvalidPlate;
				if (bIsUtf8)
					plate = Utf8ToAnsi(szInvalidPlate);
				g_AppConfig.arrInvalidPlate.push_back(plate);
				console->info("屏蔽包含 [{}] 的车牌结果", plate);
			}
		}
		ifs.close();
	}
	else
		console->info("读取invalid_plate.txt失败");

	ifs.open("match_plate.csv");
	if (ifs.is_open())
	{
		char szLine[500] = {};
		while (!ifs.eof())
		{
			ifs.getline(szLine, 500);
			if (strlen(szLine) > 0)
			{
				//TRUCKNO-作业号,TRUCKPLATENO-车牌号,RFIDNO-RFID卡号,TRUCKTYPE-拖车类型（O外拖-I内托）,CREATOR,CREATETIME,MODIFIER,MODIFIERTIME,TRUCKROOFNO,TRUCKID
				string strLine = szLine;
				vector<string> arrSep;
				if (SepString(strLine, arrSep, ',') > 3)
				{
					PlateMatch item;
					item.truck_num = arrSep[0];
					//将汉字编码格式转换
					item.plate_num = Utf8ToAnsi(arrSep[1].c_str());
					item.type = arrSep[3];
					g_AppConfig.arrPlateMatch.push_back(item);
				}
			}
		}
		console->info("读取拖车对应表 {} 条数据", g_AppConfig.arrPlateMatch.size());
		ifs.close();
	}
	else
		console->info("读取match_plate.csv失败");
}
