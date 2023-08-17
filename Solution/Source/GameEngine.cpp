// GameEngine.cpp : Defines the entry point for the application.
//

#include "GameEngine.h"

#include <windows.h>


int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MessageBox(0, "Hello!", "Engine", MB_OK | MB_ICONINFORMATION);
	return 0;
}
