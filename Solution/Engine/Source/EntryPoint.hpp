#pragma once

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"

#if ENGINE_PLATFORM_WINDOWS

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
	
namespace Engine {
	extern int Run(HINSTANCE Instance);
}

#endif // ENGINE_PLATFORM_WINDOWS

