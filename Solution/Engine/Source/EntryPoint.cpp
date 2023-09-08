#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"

#pragma warning (push, 0)
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#pragma warning (pop)

internal_static void SetupConsole()
{
	AllocConsole();
	SetConsoleTitleA("Engine Console");
	typedef struct { char* _ptr; int _cnt; char* _base; int _flag; int _file; int _charbuf; int _bufsiz; char* _tmpfname; } FILE_COMPLETE;
	*reinterpret_cast<FILE_COMPLETE*>(stdout) = *reinterpret_cast<FILE_COMPLETE*>(_fdopen(_open_osfhandle(reinterpret_cast<i64>(GetStdHandle(STD_OUTPUT_HANDLE)), _O_TEXT), "w"));
	*reinterpret_cast<FILE_COMPLETE*>(stderr) = *reinterpret_cast<FILE_COMPLETE*>(_fdopen(_open_osfhandle(reinterpret_cast<i64>(GetStdHandle(STD_ERROR_HANDLE)), _O_TEXT), "w"));
	*reinterpret_cast<FILE_COMPLETE*>(stdin) = *reinterpret_cast<FILE_COMPLETE*>(_fdopen(_open_osfhandle(reinterpret_cast<i64>(GetStdHandle(STD_INPUT_HANDLE)), _O_TEXT), "r"));
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	setvbuf(stdin, nullptr, _IONBF, 0);
}

internal_static void Launch(HINSTANCE Instance)
{
	SetupConsole();

	Engine::Log::Init();

	Engine::Run(Instance);
}

#if ENGINE_PLATFORM_WINDOWS

int CALLBACK WinMain(HINSTANCE Instance, [[maybe_unused]] HINSTANCE PrevInstance, [[maybe_unused]] LPSTR CommandLine, [[maybe_unused]] int ShowCode)
{

	Launch(Instance);
}

#endif