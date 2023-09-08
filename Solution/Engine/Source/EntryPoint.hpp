#pragma once


#if ENGINE_PLATFORM_WINDOWS

#pragma warning (push, 0)
#include <Windows.h>
#pragma warning (pop)
	
namespace Engine {
	extern int Run(HINSTANCE Instance);
}

#endif // ENGINE_PLATFORM_WINDOWS

