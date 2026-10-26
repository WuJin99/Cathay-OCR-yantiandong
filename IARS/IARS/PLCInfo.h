#ifndef PLCINFO_H
#define  PLCINFO_H
#include "Console.h"
#include "Utils.h"
#include "AppConfig.h"

const int PLC_MESSAGE_SIZE = 68;    // PLC 数据消息大小
const int OCR_TO_PLC_MESSAGE_SIZE = 0;
struct PLCMESSAGE       // PLC消息
{
    int64_t rtime; // 通讯收到完整帧PLC数据 时间戳
    uint8_t data[PLC_MESSAGE_SIZE];
};

//wxm 2020.12.3 卡塔尔项目通过plc发送ocr识别结果
struct OCRToPlc
{

	unsigned short usMsgNumber;		//消息编号,每次发送就自增1
	unsigned short taskId;			//作业任务编号
	unsigned short resultType;		//结果类型
	unsigned short usContCount;	    //箱个数
	std::string  strTruckNo;		//内集卡车号
	std::string  strContNo;		    //箱号
	std::string  strContIso;		//iso
	std::string  strPlate;          //外集卡车牌号
	unsigned short doorOpen;		//箱门打开
	unsigned short doorDir;			//箱门方向

	enum
	{
		//DOOR_DIR
		DOOR_DIR_NONE = 0,
		DOOR_DIR_FRONT,     //前
		DOOR_DIR_BACK,   //后
		//DOOR_STATE
		DOOR_NONE = 0,
		DOOR_CLOSE,
		DOOR_OPEN,
		//RESULT_TYPE
		RESULT_TYPE_NONE = 0,
		RESULT_TYPE_PMN,     //车号
		RESULT_TYPE_CN,   //箱号
		RESULT_TYPE_ALL = 4,      //车号加箱号
	};

	OCRToPlc() : usMsgNumber(0), usContCount(0){}

	//将结构序列化到buffer
	int SeriralToBuffer(unsigned char * const pszBuffer)
	{
		memset(pszBuffer,0,OCR_TO_PLC_MESSAGE_SIZE);

		int nOffset = 0;
		pszBuffer[nOffset++] = 0xFE;
		pszBuffer[nOffset++] = 0xEF;

		//消息编号
		memcpy(pszBuffer + nOffset, &usMsgNumber, 2);
		nOffset += 2;

		//备用
		nOffset += 6;

		//任务号
		memcpy(pszBuffer + nOffset, &taskId, 2);
		nOffset += 2;

		//结果类型
		memcpy(pszBuffer + nOffset, &resultType, 2);
		nOffset += 2;

		//集装箱个数
		memcpy(pszBuffer + nOffset, &usContCount, 2);
		nOffset += 2;

		//第一个车号	6	strTruck1_No	字符串, 以'\0'结束
		memcpy(pszBuffer + nOffset, strTruckNo.c_str(), strTruckNo.length());
		nOffset += 14;

		//第一个箱号	12	strCont1_No	字符串, 以'\0'结束
		memcpy(pszBuffer + nOffset, strContNo.c_str(), strContNo.length());
		nOffset += 12;

		//第一个箱：iso	6	strCont1_Iso	字符串, 以'\0'结束
		memcpy(pszBuffer + nOffset, strContIso.c_str(), strContIso.length());
		nOffset += 6;

		//车牌	18		字符串, 以'\0'结束，车牌中含有中文，需要转换为UNICODE编码后再序列化
		if (strPlate.length())
		{
			wchar_t wszPlate[100] = {};
			int nLen = MultiByteToWideChar(CP_ACP, 0, strPlate.c_str(), strPlate.length(), wszPlate, 100);
			memcpy(pszBuffer + nOffset, wszPlate, nLen * 2);
		}
		nOffset += 18;

		//箱门打开
		memcpy(pszBuffer + nOffset, &doorOpen, 2);
		nOffset += 2;

		//箱门方向
		memcpy(pszBuffer + nOffset, &doorDir, 2);
		nOffset += 2;

		//备用
		nOffset += 8;

		//除消息头、消息尾，中间数据将高低位取反
		SwapByte(pszBuffer + 2, 80 - 4);

		//报文结束符	1	message_tail	0x0D
		pszBuffer[nOffset++] = 0x0D;
		//报文结束符	1	message_tail	0x0A
		pszBuffer[nOffset++] = 0x0A;

		return nOffset;
	}
	std::string to_string()
	{
		std::stringstream ss;
		ss << "\n";
		ss << std::setw(14) << "消息号: " << usMsgNumber << "\n";
		ss << std::setw(14) << "作业号: " << taskId << "\n";
		ss << std::setw(14) << "结果类型: " << GetResultType() << "\n";
		ss << std::setw(14) << "箱个数: " << (int)usContCount << "\n";
		ss << std::setw(14) << "车号: " << strTruckNo << "\n";
		ss << std::setw(14) << "箱号: " << strContNo << "\n";
		ss << std::setw(14) << "ISO: " << strContIso << "\n";
		ss << std::setw(14) << "车牌: " << strPlate << "\n";
		ss << std::setw(14) << "箱门打开: " << GetDoorOpen() << "\n";
		ss << std::setw(14) << "箱门方向: " << GetDoorDir() << "\n";
		return ss.str();
	}
	void Clear()
	{
		//除消息序号外，清空其他字段
		taskId = 0;
		usContCount = 0;
		doorDir = 0;
		doorOpen = 0;
		resultType = 0;

		strTruckNo = "";
		strContNo = "";
		strContIso = "";
		strPlate = "";
	}
	std::string GetResultType()
	{
		switch (resultType)
		{
		default:
		{
			return fmt::format("Unknown({})", resultType);
		}
		case RESULT_TYPE_NONE:
			return "NONE(0)";
		case RESULT_TYPE_PMN:
			return "PMN(1)";
		case RESULT_TYPE_CN:
			return "CN(2)";
		case RESULT_TYPE_ALL:
			return "ALL(4)";
		}
	}
	std::string GetDoorOpen()
	{
		switch (doorOpen)
		{
		default:
		{
			return fmt::format("Unknown({})", doorOpen);
		}
		case DOOR_NONE:
			return "NONE(0)";
		case DOOR_CLOSE:
			return "CLOSE(1)";
		case DOOR_OPEN:
			return "OPEN(2)";
		}
	}
	std::string GetDoorDir()
	{
		switch (doorDir)
		{
		default:
		{
			return fmt::format("Unknown({})", doorDir);
		}
		case DOOR_DIR_NONE:
			return "NONE(0)";
		case DOOR_DIR_FRONT:
			return "FRONT(1)";
		case DOOR_DIR_BACK:
			return "BACK(2)";
		}
	}
	bool operator==(const OCRToPlc& r)const
	{
		return taskId == r.taskId &&			//作业任务编号
		resultType == r.resultType &&		//结果类型
		usContCount == r.usContCount &&	    //箱个数
		strTruckNo == r.strTruckNo &&		//内集卡车号
		strContNo == r.strContNo &&		    //箱号
		strContIso == r.strContIso &&		//iso
		strPlate == r.strPlate &&          //外集卡车牌号
		doorOpen == r.doorOpen &&		//箱门打开
		doorDir == r.doorDir;			//箱门方向
	}
	bool operator!=(const OCRToPlc& r)const
	{
		return !(*this == r);			//箱门方向
	}
};

struct PLCInfo              // PLC信息
{

    enum LOCK_STATUS
    {
		LOCK_INVALID = 0,
        UNLOCKED = 1,       // 吊具开锁
        LOCKED = 2,         // 吊具闭锁
    };
    enum CATCH_STATUS
    {
        CATCHING = 2,       // 着箱
        NON_CATCHING = 1,   // 非着箱
    };


    enum SPREADER_STATUS
    {
		SPREADER_INVALID = 0x0,//未知类型
		SINGLE20 = 0x1,      // 单吊具，做1个20尺
		SINGLE40 = 0x2,      // 单吊具，做1个40尺
		SINGLE45 = 0x3,      // 单吊具，做1个45尺
		DOUBLE = 0x4,        // 单吊具，做2个20尺
    };
	enum CN_POSITION
	{
		CN_POS_INVALID = 0x00,    // 无效
		CN_POS_F = 0x1,      // 压前
		CN_POS_M = 0x2,      // 压中
		CN_POS_B = 0x3      // 压后
	};

	enum CPS_STATUS
	{
		SEA_CPS_NO_FINISH = 0x0,			// 0=未完成
		SEA_CPS_FINISH = 0x1,			//1=对位完成
	};
	enum TASK_STATE
	{
		TASK_STATE_INVALID = 0x0,        // 无效
		TASK_STATE_START = 1,      // 任务激活
		TASK_STATE_EMPTY = 11,      // 空跑完成
		TASK_STATE_CATCH_CN = 21,      //抓箱完成
		TASK_STATE_RELEASE_CN = 31,      //放箱完成
		TASK_STATE_TASK_DROP = 91,      //任务丢弃
		TASK_STATE_END = 99,      //任务完成
	};
	enum TRUCK_TYPE
	{
		TRUCK_TYPE_INVALID = 0x0,        // 无效
		TRUCK_TYPE_INNER = 0x1,      // 内集卡
		TRUCK_TYPE_OUTER = 0x2,      // 外集卡
		TRUCK_TYPE_AGV = 0x3,      // 平板车
	};
	enum WORK_TYPE
	{
		WORK_TYPE_INVALID = 0x0,        // 无效
		WORK_TYPE_IN = 0x1,      // 进箱
		WORK_TYPE_OUT = 0x2,      // 出箱
	};




	enum TRUCK_DIRCTION
	{
		QC_INVALID = 0x0,        // 无效
		QC_LEFT = 0x1,      // QC司机左手方向
		QC_RIGHT = 0x2      // QC司机左手方向
	};

	enum CPS_CASE
	{
		CPS_CASE_INVALID = 0x00,    // 无效
		CPS_CASE_YES = 0x1,      // 带箱
		CPS_CASE_NO = 0x2,      // 不带箱
	};

	enum RESULT_TYPE
	{
		RESULT_TYPE_INVALID = 0,    // 无效
		RESULT_TYPE_CORRECT = 1,      // 正确
		RESULT_TYPE_ERROR = 2,      // 错误
	};
	enum TASK_TYPE
	{
		//1=内集卡车号；2=外集卡车号；4=箱号  8=内集卡车号+箱号  16=外集卡车号+箱号
		TASK_TYPE_INVALID = 0,    // 无效
		TASK_TYPE_TRUCK_NO = 1,      // 内集卡车号
		TASK_TYPE_PLATE_NO = 2,      // 外集卡车号
		TASK_TYPE_CN = 4,      // 箱号
		TASK_TYPE_TRUCK_NO_CN = 8,      // 内集卡车号+箱号
		TASK_TYPE_PLATE_NO_CN = 16,     //外集卡车号+箱号
	};


	enum HOIST_STATUS       // 起升动作
	{
		HOIST_INVALID = 0,  // 起升状态数据无效
		HOIST_UP = 1,       // 起升上升
		HOIST_DOWN = 2,     // 起升下降
		HOIST_STOP = 4,     // 起升停止
	};

	enum TROLLY_STATUS      // 小车动作
	{
		INVALID = 0x0,      // 无效
		FOREWORD = 0x1,     // 向前
		BACKWORD = 0x2,     // 向后
		STOP = 0x4,         // 停止
	};

	enum GANTRY_STATUS      // 大车动作
	{
		GANTRY_INVALID = 0x0,      // 无效
		GANTRY_LEFT = 0x1,     // 向左
		GANTRY_RIGHT = 0x2,     // 向右
		GANTRY_STOP = 0x4,         // 停止
	};



	//QB 洋浦港PLC协议 2021.12.5
	//enum CPS_STATUS
	//{
	//	SEA_CPS_NO_TRUCK = 0x50,		//海侧CPS对位状态 0=无集卡,,,，
	//	SEA_CPS_ING = 0x51,				//1=正在对位
	//	SEA_CPS_INVALID = 0x52,			//2=无效
	//	SEA_CPS_FINISH = 0x53,			//3=对位完成
	//};

	//QB 洋浦港PLC协议 2021.12.5
	//enum TRUCK_DIRCTION
	//{
	//	QC_INVALID = 0x00,    // 无效
	//	QC_LEFT = 0x50,      // QC司机左手方向
	//	QC_RIGHT = 0x51      // QC司机左手方向
	//};

	PLCMESSAGE plc_msg;
	string  uuid;					//消息编号
	string timeStamp;				// PLC数据接收时间戳
	string taskId;					// 任务号
	string craneId;					// 设备号
	string truckNumStr;				// 任务集卡号
	string containerNo1;			// 任务箱1
	string containerNo2;			// 任务箱2
	unsigned short taskState;		// 作业任务状态,1:任务激活, 11:空跑完成, 21:抓箱完成, 31 : 放箱完成, 91 : 任务丢弃 99 : 任务结束
	int gantryValue;			// 大车编码器值   二进制，单位m
	unsigned short trollyValue;			// 小车编码器   二进制，单位m
	unsigned short hoistValue;			// 起升编码器   二进制，单位m

	unsigned short gantrySpeed;			// 大车速度   二进制，单位m
	unsigned short trollySpeed;			// 小车速度   二进制，单位m
	unsigned short hoistSpeed;			// 起升速度   二进制，单位m
	unsigned short lockStatus;				// 开闭锁状态   0=无效;1=开锁;2=闭锁
	unsigned short catchStatus;			// 着箱状态  0=无效;1=着箱;2=非着箱
	unsigned short spreaderMode;			// 吊具类型 0=无效;1=20ft;2=40ft;3=双箱
	unsigned short workLine;			//作业车道
	unsigned short  truckType;  	//0=无效 1=内集卡 2=外集卡;3=无人平板车
	unsigned short cpsStatus;			//集卡对位状态，0：未到位；1：已到位
	float cpsDistance;			//集卡对位偏差距离，单位 mm
	unsigned short workType;			//任务类型，0：非集装箱任务；1进箱任务；2：出箱任务
	unsigned short cpsCnPos;			//集装箱压箱位置，0 未知，1 前(靠近车头)，2 中，3 后
	unsigned int hoistToCont;			// 吊具到箱顶距离   二进制，单位m

	unsigned short hasTruck[3];//新定义接口，表示实时有车车道；下标对应车道号，[0]无效，[1]=1车道，[2]=2车道
	// 0:集卡停止 1:集卡离开 2:集卡移动

	unsigned short hoistStatus;			// 起升状态    0=无效;1=上升;2=下降;3=停止
	unsigned short trollyStatus;			// 小车运动状态  0=无效；1=前行；2=后行；3=停止
	unsigned short gantryStatus;			// 大车运行状态 0=无效；1=左行；2=右行；3=停止

	unsigned short workLine1;			//作业车道1 海侧  0=无效;大于0有效
	unsigned short workLine2;			//作业车道2 陆侧  0=无效;大于0有效

	unsigned short cpsDirection;		//海侧车头方向   0=无效;1=左;2=右

	void ParseMsg(const ptree& plc);
	unsigned short ParseShort(const unsigned char *pBuf, int& offset);
	int ParseInt(const unsigned char *pBuf, int& offset);

    std::string GetSpreadMode() const   // 根据吊具宽度返回箱型
    {
        const std::map<int, std::string> m
        {
			{ SPREADER_INVALID, "UNKNOWN" },
			 { SINGLE20, "SINGLE20" },
			{ SINGLE40, "SINGLE40" },
			{ SINGLE45, "SINGLE45" },
			{ DOUBLE, "DOUBLE" },
        };
        if (m.find(spreaderMode) != m.end())
        {
            return m.at(spreaderMode);
        }
        return fmt::format("未知({})", spreaderMode);
    }
	std::string GetTaskState() const   // 作业任务状态
	{
		const std::map<int, std::string> m
		{
			{ TASK_STATE_INVALID, "INVALID" },
		    {TASK_STATE_START, "任务激活" },
			{ TASK_STATE_EMPTY, "空跑完成" },
			{ TASK_STATE_CATCH_CN, "抓箱完成" },
			{ TASK_STATE_RELEASE_CN, "放箱完成" },
			{ TASK_STATE_TASK_DROP, "任务丢弃" },
			{ TASK_STATE_END, "任务完成" },

		};
		if (m.find(taskState) != m.end())
		{
			return m.at(taskState);
		}
		return fmt::format("未知({})", taskState);
	}

    std::string GetCatchStatus() const   // 返回着箱状态
    {
        const std::map<int, std::string> m
        {
            { CATCHING, "着箱" },
            { NON_CATCHING, "非着箱" }
        };
        if (m.find(catchStatus) != m.end())
        {
            return m.at(catchStatus);
        }
        return fmt::format("未知({})", catchStatus);
    }
    std::string GetLockStatus() const    // 返回开闭锁状态
    {
        const std::map<int, std::string> m
        {
			{ LOCK_INVALID, "无效"},
			{ LOCKED, "闭锁" },
			{ UNLOCKED, "开锁" }
        };
        if (m.find(lockStatus) != m.end())
        {
            return m.at(lockStatus);
        }
        return fmt::format("未知({})", lockStatus);
    }



	std::string GetResultStr(int nResult) const  //返回车号或箱号结果
	{
		if (nResult == 1)
		{
			return "正确";
		}
		if (nResult == 2)
		{
			return "错误";
		}
		if (nResult == 0)
		{
			return "无效";
		}
		return fmt::format("未知({})", nResult);
	}
	std::string GetCpsStatus() const  //返回集卡对位状态
	{
		const std::map<int, std::string> m
		{
			{ SEA_CPS_NO_FINISH, "对位未完成" },
			{ SEA_CPS_FINISH, "对位完成" }
		};
		if (m.find(cpsStatus) != m.end())
		{
			return m.at(cpsStatus);
		}
		return fmt::format("未知({})", cpsStatus);
	}
	std::string GetCpsStatusLetter() const  //返回集卡对位状态
	{
		const std::map<int, std::string> m
		{
			{ SEA_CPS_NO_FINISH, "NO" },
			{ SEA_CPS_FINISH, "DONE" }
		};
		if (m.find(cpsStatus) != m.end())
		{
			return m.at(cpsStatus);
		}
		return fmt::format("U{}", cpsStatus);
	}




	std::string GetCpsCasePos() const  //返回压箱位置
	{
		const std::map<int, std::string> m
		{
			{ CN_POS_INVALID, "无效" },
			{ CN_POS_F, "压前" },
			{ CN_POS_M, "压中" },
			{ CN_POS_B, "压后" }
		};
		if (m.find(cpsCnPos) != m.end())
		{
			return m.at(cpsCnPos);
		}
		return fmt::format("未知({})", cpsCnPos);
	}
	std::string GetCpsCasePosLetter() const  //返回压箱位置
	{
		const std::map<int, std::string> m
		{
			{ CN_POS_INVALID, "NONE" },
			{ CN_POS_F, "Front" },
			{ CN_POS_M, "Middle" },
			{ CN_POS_B, "Back" }

		};
		if (m.find(cpsCnPos) != m.end())
		{
			return m.at(cpsCnPos);
		}
		return fmt::format("U{}", cpsCnPos);
	}
	std::string GetWorkType(unsigned short result) const
	{//任务类型，0：非集装箱任务；1进箱任务；2：出箱任务
		const std::map<int, std::string> m
		{
			{ WORK_TYPE_INVALID, "无效" },
			{ WORK_TYPE_IN, "进箱" },
			{ WORK_TYPE_OUT, "出箱" }
		};
		if (m.find(result) != m.end())
		{
			return m.at(result);
		}
		return fmt::format("未知({})", result);
	}
	std::string GetResultType(unsigned short result) const
	{
		const std::map<int, std::string> m
		{
			{ RESULT_TYPE_INVALID, "无效" },
			{ RESULT_TYPE_CORRECT, "正确" },
			{ RESULT_TYPE_ERROR, "错误" }
		};
		if (m.find(result) != m.end())
		{
			return m.at(result);
		}
		return fmt::format("未知({})", result);
	}

	std::string GetTruckType(unsigned short result) const
	{

		const std::map<int, std::string> m
		{
			{ TRUCK_TYPE_INVALID, "无效" },
			{ TRUCK_TYPE_INNER, "内集卡" },
			{ TRUCK_TYPE_OUTER, "外集卡" },
			{ TRUCK_TYPE_AGV, "平板车" }
		};
		if (m.find(result) != m.end())
		{
			return m.at(result);
		}
		return fmt::format("未知({})", result);
	}


	/*bool operator == (PLCInfo &p)
	{
		return this->gantryStatus == p.gantryStatus
			&& this->trollyStatus == p.trollyStatus
			&& this->hoistStatus == p.hoistStatus
			&& this->lockStatus == p.lockStatus
			&& this->catchStatus == p.catchStatus
			&& this->spreaderMode == p.spreaderMode
			&& this->gantryValue == p.gantryValue
			&& this->trollyValue == p.trollyValue
			&& this->hoistValue == p.hoistValue

			&& this->workLine1 == p.workLine1
			&& this->workLine2 == p.workLine2
			&& this->cpsStatus == p.cpsStatus
			&& this->cpsDirection == p.cpsDirection
			&& this->cpsCnPos == p.cpsCnPos
			&& this->cpsDistance == p.cpsDistance

			&& this->pmnResult == p.pmnResult
			&& this->plateResult == p.plateResult
			&& this->cnResult == p.cnResult
			&& this->taskType == p.taskType
			&& this->taskId == p.taskId
			&& this->truckNumStr == p.truckNumStr
			&& this->truckType == p.truckType;
	}*/

	/*bool operator != (PLCInfo &p)
	{
		return !(*this == p);
	}*/
};

template<typename OStream>
OStream& operator << (OStream& out, const PLCInfo& plcInfo)
{
	out << "\n";
	out << std::setw(14) << "消息编号: " << plcInfo.uuid ;
	out << "\n";
	out << std::setw(14) << "时间: " << plcInfo.timeStamp;
	out << "\n";
	out << std::setw(14) << "任务号: " << plcInfo.taskId;
	out << "\n";
	out << std::setw(14) << "设备号: " << plcInfo.craneId;
	out << "\n";
	out << std::setw(14) << "任务集卡号: " << plcInfo.truckNumStr;
	out << "\n";
	out << std::setw(14) << "作业箱号1: " << plcInfo.containerNo1;
	out << "\n";
	out << std::setw(14) << "作业箱号2: " << plcInfo.containerNo2;
	out << "\n";
	out << std::setw(14) << "任务状态: " << plcInfo.GetTaskState();
	out << "\n";
	out << std::setw(14) << "起升编码器: " << plcInfo.hoistValue ;
	out << "\n";
	out << std::setw(14) << "小车编码器: " << plcInfo.trollyValue;
	out << "\n";
	out << std::setw(14) << "大车编码器: " << plcInfo.gantryValue ;
	out << "\n";
	out << std::setw(14) << "起升速度: " << plcInfo.hoistSpeed ;
	out << "\n";
	out << std::setw(14) << "小车速度: " << plcInfo.trollySpeed ;
	out << "\n";
	out << std::setw(14) << "大车速度: " << plcInfo.gantrySpeed ;
	out << "\n";
	out << std::setw(14) << "吊具到箱顶距离: " << plcInfo.hoistToCont ;
	out << "\n";
//
	out << std::setw(14) << "开闭锁状态: " << plcInfo.GetLockStatus() << "(" << plcInfo.lockStatus << ")";
	out << "\n";
	out << std::setw(14) << "着箱状态: " << plcInfo.GetCatchStatus() << "(" << plcInfo.catchStatus << ")";
	out << "\n";
	out << std::setw(14) << "吊具类型: " << plcInfo.GetSpreadMode() << "(" << plcInfo.spreaderMode << ")";
	out << "\n";
	out << std::setw(14) << "作业车道: " << plcInfo.workLine ;
	out << "\n";
//2026.2
	out << std::setw(14) << "1车道有车: " <<plcInfo.hasTruck[1];
	out << "\n";
	out << std::setw(14) << "2车道有车: " << plcInfo.hasTruck[2];
	out << "\n";
	out << std::setw(14) << "集卡类型 " << plcInfo.GetTruckType(plcInfo.truckType) ;
	out << "\n";

//	//unsigned short cpsStatus;			//集卡对位状态，0：未到位；1：已到位
////float cpsDistance;			//集卡对位偏差距离，单位 mm
////unsigned short workType;			//任务类型，0：非集装箱任务；1进箱任务；2：出箱任务
////unsigned short cpsCnPos;			//集装箱压箱位置，0 未知，1 前(靠近车头)，2 中，3 后
////float hoistToCont;			// 吊具到箱顶距离   二进制，单位m
//
	out << std::setw(14) << "压箱位置: " << plcInfo.GetCpsCasePos() << "(" << plcInfo.cpsCnPos << ")";
	out << "\n";
    	out << std::setw(14) << "压箱位置: " << plcInfo.GetCpsCasePos() << "(" << plcInfo.cpsCnPos << ")";
    	out << "\n";

    	out << std::setw(14) << "对位状态: " << plcInfo.GetCpsStatus() << "(" << plcInfo.cpsStatus << ")";
    	out << "\n";
    	out << std::setw(14) << "对位距离: " << plcInfo.cpsDistance ;
    	out << "\n";
    	out << std::setw(14) << "作业类型: " << plcInfo.GetWorkType(plcInfo.workType);
    	out << "\n";

    return out;
}


extern boost::mutex g_PlcUpdateMutex; // 全局互斥锁.

extern boost::condition_variable g_PlcUpdateCond; // 全局条件变量.
//
//extern boost::mutex g_PlcUpdateMutexOne; // 全局互斥锁.

//extern boost::condition_variable g_PlcUpdateCondOne; // 全局条件变量.

extern std::map<std::string, PLCInfo*> g_plcSubscriber;

void SubscribePLC(std::string camIP, PLCInfo &plcInfo);

void UnSubscribePLC(std::string camIP);

extern OCRToPlc g_OCRToPlc;
extern std::mutex g_mtxOCRToPlc;

//记录发生吊具闭锁时的编码器值
extern std::atomic<int> g_nLockTrallyValue;
extern std::atomic<int> g_nLockHoistValue;
//extern std::atomic<int> g_nLockHoistValue1;

#endif
