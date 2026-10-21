#include "Monitor.h"
//对应源文件 monitor.cpp
#pragma comment(lib, "nvml.lib")
#include "Console.h"

const int Byte2MB = 1024 * 1024;
__int64 CompareFileTime(FILETIME time1, FILETIME time2)
{
	__int64 a = time1.dwHighDateTime << 32 | time1.dwLowDateTime;
	__int64 b = time2.dwHighDateTime << 32 | time2.dwLowDateTime;

	return (b - a);
}
int Monitor::type = 0;
std::deque <double> Monitor::cpuDeque(30,0.0);
std::mutex Monitor::cpuMutex;
double Monitor::tempCpuRate=0.0;
double Monitor::GetCpuRate() 
{

	HANDLE hEvent;
	BOOL res;
	FILETIME preidleTime;
	FILETIME prekernelTime;
	FILETIME preuserTime;
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

	res = GetSystemTimes(&idleTime, &kernelTime, &userTime);
	preidleTime = idleTime;
	prekernelTime = kernelTime;
	preuserTime = userTime;

	hEvent = CreateEventA(NULL, FALSE, FALSE, NULL); // 初始值为 nonsignaled ，并且每次触发后自动设置为nonsignaled  

	WaitForSingleObject(hEvent, 1000);
	res = GetSystemTimes(&idleTime, &kernelTime, &userTime);

	__int64 idle = CompareFileTime(preidleTime, idleTime);
	__int64 kernel = CompareFileTime(prekernelTime, kernelTime);
	__int64 user = CompareFileTime(preuserTime, userTime);


	preidleTime = idleTime;
	prekernelTime = kernelTime;
	preuserTime = userTime;

	if (kernel + user == 0)
	{
		return 0;
	}
	cpuMutex.lock();
	double cpuRate =  (kernel + user - idle) * 100.0 / (kernel + user);
	//测试发现cpuRate有负数
	if (cpuRate > 0)
	{
			cpuDeque.pop_front();
			if (cpuRate > 100.0)
			{
				cpuDeque.push_back(100.0);
			}
			else
			{
				cpuDeque.push_back(cpuRate);
			}
	}

	double tot = 0.0;
	for (auto &a : cpuDeque)
	{
		tot += a;
	}
	cpuMutex.unlock();
	return tot / cpuDeque.size() * 1.0;
}

//bool Monitor::GetGpuCount(unsigned int &count)
//{
//
//	if (NVML_SUCCESS != nvmlInit())
//	{
//		return false;
//	}
//
//
//	if (NVML_SUCCESS != nvmlDeviceGetCount(&count))
//	{
//		return false;
//	}
//
//	return true;
//}

bool Monitor::GetGpuRate(float &rate)
{
	nvmlReturn_t result;
	unsigned int device_count, i;

	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		console->error("GPU nvmlInit {}", nvmlErrorString(result));
		return false;
	}
	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		console->error("GPU nvmlDeviceGetCount {}", nvmlErrorString(result));
		nvmlShutdown();
		return false;
	}

	bool bGetValidRate = false;
	//如果有多块显卡可以打开注释，并在下面Index改为对应的index
	for (i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;
		char name[NVML_DEVICE_NAME_BUFFER_SIZE] = {};
		nvmlPciInfo_t pci;
		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result)
		{
			console->error("GPU nvmlDeviceGetHandleByIndex {}", nvmlErrorString(result));
			continue;
		}
		result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
		if (NVML_SUCCESS != result)
		{
			console->error("GPU nvmlDeviceGetName {}", nvmlErrorString(result));
			continue;
		}
		//使用率
		/*nvmlUtilization_t utilization;
		result = nvmlDeviceGetUtilizationRates(device, &utilization);
		if (NVML_SUCCESS != result)
		{
			console->error("GPU nvmlDeviceGetUtilizationRates {}", nvmlErrorString(result));
		}
		else
		{
			console->info("GPU nvmlDeviceGetUtilizationRates {} {}", utilization.gpu, utilization.memory);
		}*/
		//显存
		nvmlMemory_t memory;
		result = nvmlDeviceGetMemoryInfo(device, &memory);
		if (NVML_SUCCESS != result)
		{
			console->error("GPU nvmlDeviceGetMemoryInfo {}", nvmlErrorString(result));
			continue;
		}
		rate = 100.0 * (float)memory.used / (float)memory.total;
		if (rate > 0 && rate <= 100)
		{
			bGetValidRate = true;
			break;
		}
	}
	nvmlShutdown();
	return bGetValidRate;
}


std::pair<byte, unsigned short> Monitor::GetDiskRate() 
{

	ULARGE_INTEGER FreeBytesAvailableC, TotalNumberOfBytesC, TotalNumberOfFreeBytesC;
	ULARGE_INTEGER FreeBytesAvailableD, TotalNumberOfBytesD, TotalNumberOfFreeBytesD;
	ULARGE_INTEGER FreeBytesAvailableE, TotalNumberOfBytesE, TotalNumberOfFreeBytesE;
	ULARGE_INTEGER FreeBytesAvailableF, TotalNumberOfBytesF, TotalNumberOfFreeBytesF;

	GetDiskFreeSpaceEx(_T("C:"), &FreeBytesAvailableC, &TotalNumberOfBytesC, &TotalNumberOfFreeBytesC);
	GetDiskFreeSpaceEx(_T("D:"), &FreeBytesAvailableD, &TotalNumberOfBytesD, &TotalNumberOfFreeBytesD);
	GetDiskFreeSpaceEx(_T("E:"), &FreeBytesAvailableE, &TotalNumberOfBytesE, &TotalNumberOfFreeBytesE);
	GetDiskFreeSpaceEx(_T("F:"), &FreeBytesAvailableF, &TotalNumberOfBytesF, &TotalNumberOfFreeBytesF);
	float totalHardDisk = GB(TotalNumberOfBytesC) + GB(TotalNumberOfBytesD) + GB(TotalNumberOfBytesE) + GB(TotalNumberOfBytesF);
	float freeHardDisk = GB(TotalNumberOfFreeBytesC) + GB(TotalNumberOfFreeBytesD) + GB(TotalNumberOfFreeBytesE) + GB(TotalNumberOfFreeBytesF);

	double hardDiskUsage = (1 - freeHardDisk / totalHardDisk) * 100;
	auto res = std::make_pair((byte)hardDiskUsage, unsigned short(freeHardDisk));
	return res;
}

double Monitor::GetMemoryRate() 
{

	MEMORYSTATUSEX statex;

	statex.dwLength = sizeof(statex);

	GlobalMemoryStatusEx(&statex);
	int nAvail = (int)(statex.ullAvailPhys / Byte2MB);
	int nTotal = (int)(statex.ullTotalPhys / Byte2MB);
	return 100.0 * (nTotal - nAvail) / nTotal;
}

