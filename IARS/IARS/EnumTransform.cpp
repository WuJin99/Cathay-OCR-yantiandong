#include "EnumTransform.h"
std::string EnumTransform::GetImdgStr(ImdgResult::ImdgInfo::IMDG_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("无");
		break;
	case 1:
		return std::string("爆炸品");
		break;
	case 2:
		return std::string("易燃气体");
		break;
	case 3:
		return std::string("不燃气体");
		break;
	case 4:
		return std::string("有毒气体");
		break;
	case 5:
		return std::string("易燃液体");
		break;
	case 6:
		return std::string("易燃固体");
		break;
	case 7:
		return std::string("自燃物品");
		break;
	case 8:
		return std::string("遇湿易燃物品");
		break;
	case 9:
		return std::string("氧化剂");
		break;
	case 10:
		return std::string("有机过氧化物");
		break;
	case 11:
		return std::string("有毒品");
		break;
	case 12:
		return std::string("剧毒品");
		break;
	case 13:
		return std::string("一级放射性物品");
		break;
	case 14:
		return std::string("二级放射性物品");
		break;
	case 15:
		return std::string("三级放射性物品");
		break;
	case 16:
		return std::string("腐蚀品");
		break;
	default:
		return std::string("无对应类型");
		break;
	}
}
std::string EnumTransform::GetDamageStr(DamageResult::DamageInfo::DAMAGE_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("无");
		break;
	case 1:
		return std::string("D，凹陷");
		break;
	case 2:
		return std::string("B，擦伤");
		break;
	case 3:
		return std::string("BR，破损");
		break;
	case 4:
		return std::string("DO，变形、扭曲");
		break;
	case 5:
		return std::string("K，弯曲、翘起");
		break;
	case 6:
		return std::string("H，破洞");
		break;
	case 7:
		return std::string("M，铅封丢失");
		break;
	case 8:
		return std::string("C，切口");
		break;
	case 9:
		return std::string("P，修补");
		break;
	case 10:
		return std::string("S，刮伤");
		break;
	case 11:
		return std::string("R，锈蚀");
		break;
	default:
		return std::string("无对应类型");
		break;
	}
}
std::string EnumTransform::GetDamageLevelStr(DamageResult::DamageInfo::DAMAGE_LEVEL level)
{
	switch (level)
	{
	case 0:
		return std::string("无");
		break;
	case 1:
		return std::string("轻微");
		break;
	case 2:
		return std::string("中度");
		break;
	case 3:
		return std::string("严重");
		break;
	default:
		return std::string("无对应类型");
		break;
	}
}
std::string EnumTransform::GetDoorStr(const string &dir)
{
	if (isEqual(dir,"Unknow"))
	{
		return std::string("U");
	}
	/*else if (isEqual(dir, "LEFT"))
	{
		if (isEqual(g_TaskInfo.truck_head_dir(), "LEFT"))
		{
			return std::string("F");
		}
		else if (isEqual(g_TaskInfo.truck_head_dir(), "RIGHT"))
		{
			return std::string("B");
		}
		else
		{
			return std::string("U");
		}
	}
	else if (isEqual(dir, "RIGHT"))
	{
		if (isEqual(g_TaskInfo.truck_head_dir(), "LEFT"))
		{
			return std::string("B");
		}
		else if (isEqual(g_TaskInfo.truck_head_dir(), "RIGHT"))
		{
			return std::string("F");
		}
		else
		{
			return std::string("U");
		}
	}*/
	else
	{
		return std::string("U");
	}

}
std::string EnumTransform::GetDoorOpenStr(DoorOPENResult::OPEN_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("未知");
		break;
	case 1:
		return std::string("关闭");
		break;
	case 2:
		return std::string("打开");
		break;
	default:
		return std::string("无对应类型");
		break;
	}
}

std::string EnumTransform::GetBoolDoorOpen(DoorOPENResult::OPEN_TYPE type)
{
	switch (type)
	{
	case 0:
		return std::string("未知");
		break;
	case 1:
		return std::string("N");
		break;
	case 2:
		return std::string("Y");
		break;
	default:
		return std::string("无对应类型");
		break;
	}
}

std::string EnumTransform::GetBoolSeal(bool b)
{
	return b ? "Y" : "N";
}
std::string EnumTransform::GetBoolImdg(bool b)
{
	return b ? "Y" : "N";
}

std::string EnumTransform::GetBoolDamage(bool b)
{
	return b ? "Y" : "N";
}

std::string EnumTransform::GetDoorResult(int type)
{
	switch (type)
	{
	case DoorResult::BACK_DOOR:
		return "BACK";
	case DoorResult::FRONT_DOOR:
		return "FRONT";
	case DoorResult::FLAT:
		return "FLAT";
	case DoorResult::REEFER:
		return "REEFER";
	case DoorResult::TANK:
		return "TANK";
	default:
		return "U";
	}
}

EnumTransform::EnumTransform()
{
}

EnumTransform::~EnumTransform()
{
}
