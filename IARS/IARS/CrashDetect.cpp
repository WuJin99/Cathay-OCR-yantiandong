#include "CrashDetect.h"
 

// 添加对dbghelp.lib的编译依赖  
//  
#pragma comment(lib, "dbghelp.lib")  


using namespace std;

const int MAX_ADDRESS_LENGTH = 320;
const int MAX_NAME_LENGTH = 1024;

// 崩溃信息  
//   
struct CrashInfo
{
	CHAR ErrorCode[MAX_ADDRESS_LENGTH];
	CHAR Address[MAX_ADDRESS_LENGTH];
	CHAR Flags[MAX_ADDRESS_LENGTH];
};

// CallStack信息  
//   
struct CallStackInfo
{
	CHAR ModuleName[MAX_NAME_LENGTH];
	CHAR MethodName[MAX_NAME_LENGTH];
	CHAR FileName[MAX_NAME_LENGTH];
	CHAR LineNumber[MAX_NAME_LENGTH];
};

// 安全拷贝字符串函数  
//  
void SafeStrCpy(char* szDest, size_t nMaxDestSize, const char* szSrc)
{
	if (nMaxDestSize <= 0) return;
	if (strlen(szSrc) < nMaxDestSize)
	{
		strcpy_s(szDest, nMaxDestSize, szSrc);
	}
	else
	{
		strncpy_s(szDest, nMaxDestSize, szSrc, nMaxDestSize);
		szDest[nMaxDestSize - 1] = '\0';
	}
}

// 得到程序崩溃信息  
//  
void GetCrashInfo(const EXCEPTION_RECORD *pRecord, CrashInfo& crashinfo)
{
	SafeStrCpy(crashinfo.Address, MAX_ADDRESS_LENGTH, "N/A");
	SafeStrCpy(crashinfo.ErrorCode, MAX_ADDRESS_LENGTH, "N/A");
	SafeStrCpy(crashinfo.Flags, MAX_ADDRESS_LENGTH, "N/A");

	sprintf_s(crashinfo.Address, "%08X", pRecord->ExceptionAddress);
	sprintf_s(crashinfo.ErrorCode, "%08X", pRecord->ExceptionCode);
	sprintf_s(crashinfo.Flags, "%08X", pRecord->ExceptionFlags);
}

string GetTime()
{
	SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);
	char szTime[100] = {};
	sprintf_s(szTime, "[%d-%02d-%02d %02d:%02d:%02d.%03d] ",
		sysTime.wYear, sysTime.wMonth, sysTime.wDay,
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
	return szTime;
}
// 得到CallStack信息  
//  
void GetCallStack(const CONTEXT *pContext, vector<CallStackInfo>&arrCallStackInfo, FILE *out)
{
	try
	{
		HANDLE hProcess = GetCurrentProcess();
		if (!SymInitialize(hProcess, NULL, TRUE))
		{
			fprintf_s(out, "%s GetCallStack SymInitialize %d", GetTime().c_str(), GetLastError());
			return;
		}

		CONTEXT c = *pContext;
		STACKFRAME64 sf;
		memset(&sf, 0, sizeof(STACKFRAME64));
		DWORD dwImageType = IMAGE_FILE_MACHINE_I386;

		// 不同的CPU类型，具体信息可查询MSDN  
		//  
#ifdef _M_IX86  
		sf.AddrPC.Offset = c.Eip;
		sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrStack.Offset = c.Esp;
		sf.AddrStack.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = c.Ebp;
		sf.AddrFrame.Mode = AddrModeFlat;
#elif _M_X64  
		dwImageType = IMAGE_FILE_MACHINE_AMD64;
		sf.AddrPC.Offset = c.Rip;
		sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = c.Rsp;
		sf.AddrFrame.Mode = AddrModeFlat;
		sf.AddrStack.Offset = c.Rsp;
		sf.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64  
		dwImageType = IMAGE_FILE_MACHINE_IA64;
		sf.AddrPC.Offset = c.StIIP;
		sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = c.IntSp;
		sf.AddrFrame.Mode = AddrModeFlat;
		sf.AddrBStore.Offset = c.RsBSP;
		sf.AddrBStore.Mode = AddrModeFlat;
		sf.AddrStack.Offset = c.IntSp;
		sf.AddrStack.Mode = AddrModeFlat;
#else  
#error "Platform not supported!"  
#endif  

		HANDLE hThread = GetCurrentThread();

		while (true)
		{
			// 该函数是实现这个功能的最重要的一个函数  
			// 函数的用法以及参数和返回值的具体解释可以查询MSDN  
			//  
			if (!StackWalk64(dwImageType, hProcess, hThread, &sf, &c, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
			{
				break;
			}

			if (sf.AddrFrame.Offset == 0)
			{
				break;
			}

			CallStackInfo callstackinfo;
			SafeStrCpy(callstackinfo.MethodName, MAX_NAME_LENGTH, "N/A");
			SafeStrCpy(callstackinfo.FileName, MAX_NAME_LENGTH, "N/A");
			SafeStrCpy(callstackinfo.ModuleName, MAX_NAME_LENGTH, "N/A");
			SafeStrCpy(callstackinfo.LineNumber, MAX_NAME_LENGTH, "N/A");

			BYTE symbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_NAME_LENGTH];
			IMAGEHLP_SYMBOL64 *pSymbol = (IMAGEHLP_SYMBOL64*)symbolBuffer;
			memset(pSymbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_NAME_LENGTH);

			pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);// 
			pSymbol->Size = sizeof(symbolBuffer);
			pSymbol->MaxNameLength = MAX_NAME_LENGTH - 1;

			DWORD symDisplacement = 0;

			// 得到函数名  
			//  
			if (SymGetSymFromAddr64(hProcess, sf.AddrPC.Offset, NULL, pSymbol))
			{
				SafeStrCpy(callstackinfo.MethodName, MAX_NAME_LENGTH, pSymbol->Name);
			}

			IMAGEHLP_LINE64 lineInfo;
			memset(&lineInfo, 0, sizeof(IMAGEHLP_LINE64));

			lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

			DWORD dwLineDisplacement;

			// 得到文件名和所在的代码行  
			//  
			if (SymGetLineFromAddr64(hProcess, sf.AddrPC.Offset, &dwLineDisplacement, &lineInfo))
			{
				SafeStrCpy(callstackinfo.FileName, MAX_NAME_LENGTH, lineInfo.FileName);
				sprintf_s(callstackinfo.LineNumber, "%d", lineInfo.LineNumber);
			}

			IMAGEHLP_MODULE64 moduleInfo;
			memset(&moduleInfo, 0, sizeof(IMAGEHLP_MODULE64));

			moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

			// 得到模块名  
			//  
			if (SymGetModuleInfo64(hProcess, sf.AddrPC.Offset, &moduleInfo))
			{
				SafeStrCpy(callstackinfo.ModuleName, MAX_NAME_LENGTH, moduleInfo.ModuleName);
			}
			arrCallStackInfo.push_back(callstackinfo);
		}
		SymCleanup(hProcess);
	}
	catch (std::exception& e)
	{
		fprintf_s(out, "%s GetCallStack exception %s\n", GetTime().c_str(), e.what());
	}
	catch (...)
	{
		fprintf_s(out, "%s GetCallStack exception ...\n", GetTime().c_str());
	}
}
// 处理Unhandled Exception的回调函数  
// 
class CAutoCloseFile
{
	CAutoCloseFile() { m_p = nullptr; }
public:
	CAutoCloseFile(FILE *p) { m_p = p; }
	~CAutoCloseFile()
	{
		if (m_p)
		{
			fclose(m_p);
			m_p = nullptr;
		}
	}
private:
	FILE *m_p;
};

void CreateMiniDump(PEXCEPTION_POINTERS pep, LPCTSTR strFileName, FILE* out)
{
	HANDLE hFile = CreateFile(strFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = pep;
		mdei.ClientPointers = NULL;

		/*MINIDUMP_CALLBACK_INFORMATION mci;
		mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)MiniDumpCallback;
		mci.CallbackParam = 0;*/

		MINIDUMP_TYPE dump_type = (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithProcessThreadData| MiniDumpWithThreadInfo);
		BOOL ret = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, dump_type, (pep != 0) ? &mdei : 0, NULL, NULL);

		CloseHandle(hFile);
		fprintf_s(out, "%s SaveDumpFile %s\n", GetTime().c_str(), ret ? "succ" : "fail");
	}
	else
	{
		fprintf_s(out, "%s CreateFile fail, %d\n", GetTime().c_str(), GetLastError());
	}
}

LONG _stdcall ApplicationCrashHandler(EXCEPTION_POINTERS *pException)
{
	char szFileName[64] = {};
	SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);
	sprintf_s(szFileName, "./CallStack_%d%02d%02d%02d%02d%02d.txt", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
	char szDumpFile[64] = {};
	sprintf_s(szDumpFile, "./CallStack_%d%02d%02d%02d%02d%02d.dmp", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
	FILE *out = nullptr;
	fopen_s(&out, szFileName, "a+t");
	if (!out)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}
	CAutoCloseFile auto_close_file(out);
	CreateMiniDump(pException, szDumpFile, out);
	if (!pException)
	{
		fprintf_s(out, "%s EXCEPTION_POINTERS NULL\n", GetTime().c_str());
		return EXCEPTION_EXECUTE_HANDLER;
	}
	if (!pException->ExceptionRecord)
	{
		fprintf_s(out, "%s ExceptionRecord NULL\n", GetTime().c_str());
		return EXCEPTION_EXECUTE_HANDLER;
	}
	// 确保有足够的栈空间  
	//  
#ifdef _M_IX86  
	if (pException->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
	{
		static char TempStack[1024 * 128];
		__asm mov eax, offset TempStack[1024 * 128];
		__asm mov esp, eax;
	}
#endif    

	CrashInfo crashinfo;
	GetCrashInfo(pException->ExceptionRecord, crashinfo);

	// 输出Crash信息  
	//  
	cout << "ErrorCode: " << crashinfo.ErrorCode << endl;
	cout << "Address: " << crashinfo.Address << endl;
	cout << "Flags: " << crashinfo.Flags << endl;

	fprintf_s(out, "%s =============================\n", GetTime().c_str());
	fprintf_s(out, "%s ErrorCode: %s\n", GetTime().c_str(), crashinfo.ErrorCode);
	fprintf_s(out, "%s Address: %s\n", GetTime().c_str(), crashinfo.Address);
	fprintf_s(out, "%s Flags: %s\n", GetTime().c_str(), crashinfo.Flags);

	vector<CallStackInfo> arrCallStackInfo;
	GetCallStack(pException->ContextRecord, arrCallStackInfo, out);

	// 输出CallStack  
	//  
	cout << "CallStack: " << endl;
	fprintf_s(out, "%s *************** CallStack ***************\n", GetTime().c_str());

	for (auto&callstackinfo : arrCallStackInfo)
	{
		cout << callstackinfo.MethodName << "() : [" << callstackinfo.ModuleName << "] (File: " << callstackinfo.FileName << " @Line " << callstackinfo.LineNumber << ")" << endl;

		fprintf_s(out, "%s MethodName:%s\n", GetTime().c_str(), callstackinfo.MethodName);
		fprintf_s(out, "%s ModuleName:%s\n", GetTime().c_str(), callstackinfo.ModuleName);
		fprintf_s(out, "%s FileName:%s\n", GetTime().c_str(), callstackinfo.FileName);
		fprintf_s(out, "%s LineNumber:%s\n", GetTime().c_str(), callstackinfo.LineNumber);
		fprintf_s(out, "%s -------------------\n", GetTime().c_str());
	}
	// 这里弹出一个错误对话框并退出程序  
	//  
	//FatalAppExit(-1, _T("*** Unhandled Exception! ***"));
	
	//CAutoCloseFile析构时自动关闭文件，此处不再fclose
	//fclose(out);

	return EXCEPTION_EXECUTE_HANDLER;
}