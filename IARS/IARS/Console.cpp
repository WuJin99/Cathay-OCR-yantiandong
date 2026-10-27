#include "Console.h"
#include "Utils.h"
#include "md5.hpp"

std::shared_ptr<spdlog::logger> console = spdlog::stdout_color_mt("console");
bool isExit = false;
void InitConsole(std::string logPath)
{
    logPath = logPath + "\\IARS-8917.log";

    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    // 内部版本号
	// string versionStr = "OCR-IARS-CN8917-P2301-RMG-V3.02.006.261026-001";
	// 工程要求版本号
	string versionStr = "OCR-IARS-CN8917-P2301-RMG-V3.02.005.260907-023.4.5";
    SetConsoleTitle(versionStr.c_str());

    //freopen("CONOUT$", "w", stdout);
    //freopen("CONIN$", "r", stdin);

    spdlog::drop("console");
    static std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPath, 1024 * 1024 * 8, 50));
    console = std::make_shared<spdlog::logger>("console", begin(sinks), end(sinks));
    console->set_level(spdlog::level::debug);
    console->flush_on(spdlog::level::debug);
    console->sinks()[0]->set_level(spdlog::level::trace);
    console->sinks()[1]->set_level(spdlog::level::debug);
    //console->set_pattern("[%Y-%m-%d %H:%M:%S %e] %v");
    console->debug(versionStr + " 启动");
	string strMd5 = CMD5Tool::CalcMD5("IARS.exe");
	console->warn(strMd5);

	//将版本号写入version.txt中
	string version_str = "[" + GetTimeString();
	version_str += "] version:[";
	version_str += versionStr;
	version_str += "] md5:[";
	version_str += strMd5;
	version_str += "]\n";
	WriteStringToTxt("version.txt", version_str);
}
