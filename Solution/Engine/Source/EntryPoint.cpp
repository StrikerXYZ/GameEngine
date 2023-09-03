#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

internal_static void SetupConsole()
{
	AllocConsole();
	SetConsoleTitleA("Engine Console");
	typedef struct { char* _ptr; int _cnt; char* _base; int _flag; int _file; int _charbuf; int _bufsiz; char* _tmpfname; } FILE_COMPLETE;
	*(FILE_COMPLETE*)stdout = *(FILE_COMPLETE*)_fdopen(_open_osfhandle(reinterpret_cast<u64>(GetStdHandle(STD_OUTPUT_HANDLE)), _O_TEXT), "w");
	*(FILE_COMPLETE*)stderr = *(FILE_COMPLETE*)_fdopen(_open_osfhandle(reinterpret_cast<u64>(GetStdHandle(STD_ERROR_HANDLE)), _O_TEXT), "w");
	*(FILE_COMPLETE*)stdin = *(FILE_COMPLETE*)_fdopen(_open_osfhandle(reinterpret_cast<u64>(GetStdHandle(STD_INPUT_HANDLE)), _O_TEXT), "r");
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdin, NULL, _IONBF, 0);
}

internal_static void Launch(HINSTANCE Instance)
{
	SetupConsole();

	Engine::Log::Init();

	Engine::Run(Instance);
}

#if ENGINE_PLATFORM_WINDOWS

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	Launch(Instance);
}

#endif