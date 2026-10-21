#pragma once
//头文件 monitor.h
#include <windows.h>
#include <tchar.h>
#include<iostream>
#include <deque>
#include <stdio.h>

#include <mutex>
#include "nvml.h"

#define GB(x) (((x).HighPart << 2) + ((DWORD)(x).LowPart) / 1024.0 / 1024.0 / 1024.0)

typedef unsigned char byte;
class Monitor
{
public:
	//CPU使用率 例如：43.2322
	static double GetCpuRate();
	//返回GPU显存使用率 例如：76.3443
	static bool GetGpuRate(float &rate);
	//获取显卡数量
	//bool GetGpuCount(unsigned int &count);
	std::pair<byte, unsigned short> GetDiskRate();
	//返回内存使用率 例如:46.88
	static double GetMemoryRate();
	//状态类型
	static int type;

	static std::deque <double> cpuDeque;

	static double tempCpuRate;
	static std::mutex cpuMutex;
};


