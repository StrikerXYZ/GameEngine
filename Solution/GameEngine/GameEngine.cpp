// GameEngine.cpp : Defines the entry point for the application.
//

#include <windows.h>
#include "GameEngine.h"

using namespace std;

int CALLBACK WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
)
{
	OutputDebugStringA("This is a test\n");
	return 0;
}
