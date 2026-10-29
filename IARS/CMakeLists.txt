cmake_minimum_required(VERSION 3.20)
project(IARS)
set(CMAKE_CXX_STANDARD 20)
# 核心：导出编译命令数据库
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 这里添加你的源文件，例如：
add_executable(IARS main.cpp)
