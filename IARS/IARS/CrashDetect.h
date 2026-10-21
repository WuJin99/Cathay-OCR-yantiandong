#pragma once

#include <Windows.h>  
#include <DbgHelp.h>  
#include <iostream>  
#include <vector> 

extern LONG _stdcall ApplicationCrashHandler(EXCEPTION_POINTERS *pException);


