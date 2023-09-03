#pragma once


#if ENGINE_PLATFORM_WINDOWS

#include <windows.h>
	
namespace Engine {
	extern int Run(HINSTANCE Instance);
}

#endif // ENGINE_PLATFORM_WINDOWS

