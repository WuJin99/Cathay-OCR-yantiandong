// 控制台
#ifndef CONSOLE_H
#define CONSOLE_H
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
//#include <iomanip>
#include <spdlog/fmt/ostr.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" 
#include "SyncQueue.h"
#include "ocrs.pb.h"
using namespace spdlog;
// 全局变量;
extern std::shared_ptr<spdlog::logger> console;

void InitConsole(std::string logPath);

extern bool isExit;
#endif //CONSOLE_H