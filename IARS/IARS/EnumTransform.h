#pragma once
#include "Utils.h"
class EnumTransform
{
public:
	EnumTransform();
	~EnumTransform();
public:
	static std::string GetImdgStr(ImdgResult::ImdgInfo::IMDG_TYPE type);
	static std::string GetDamageStr(DamageResult::DamageInfo::DAMAGE_TYPE type);
	static std::string GetDamageLevelStr(DamageResult::DamageInfo::DAMAGE_LEVEL level);
	static std::string GetDoorStr(const string &dir);
	static std::string GetDoorOpenStr(DoorOPENResult::OPEN_TYPE type);
	static std::string GetBoolDoorOpen(DoorOPENResult::OPEN_TYPE type);
	static std::string GetBoolSeal(bool b);
	static std::string GetBoolImdg(bool b);
	static std::string GetBoolDamage(bool b);
	static std::string GetDoorResult(int type);
};

