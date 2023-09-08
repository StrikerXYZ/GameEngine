﻿// GameEngine.cpp : Defines the entry point for the application.
//

#include "Minimal.hpp"

#pragma warning (push, 0)
#include <Windows.h>
#include <Xinput.h>
#include <stdio.h>
#pragma warning (pop)

struct Win32BitmapBuffer
{
	BITMAPINFO Info;
	void* Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

global_static bool GlobalRunning;
global_static Win32BitmapBuffer GlobalBackBuffer;

struct Win32WindowDimension
{
	int Width;
	int Height;
};

internal_static Win32WindowDimension Win32GetWindowDimension(HWND Window)
{
	Win32WindowDimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal_static void RenderGradient(Win32BitmapBuffer Buffer, int XOffset, int YOffset)
{
	//todo: test buffer pass by value performance
	u8* Row = static_cast<u8*>(Buffer.Memory);
	for (int Y = 0; Y < Buffer.Height; ++Y)
	{
		u32* Pixel = reinterpret_cast<u32*>(Row);
		for (int X = 0; X < Buffer.Width; ++X)
		{
			//Memory:		BB GG RR 00 - Little Endian
			//Registers:	xx RR GG BB
			u8 Blue = static_cast<u8>(X + XOffset);
			u8 Green = static_cast<u8>(Y + YOffset);
			*Pixel++ = static_cast<u32>((Green << 8) | Blue);
		}

		Row += Buffer.Pitch;
	}
}

internal_static void Win32ResizeDIBSection(Win32BitmapBuffer* Buffer, int Width, int Height) //DIB = Device Independant Bitmap
{
	//todo: bullet proof this - maybe free after
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4; //Need to be aligned to boundary for speed

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; //treat bitmap as top down
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	u64 BitmapMemorySize = static_cast<u64>(Buffer->Width * Buffer->Height * Buffer->BytesPerPixel);
	Buffer->Memory = VirtualAlloc(nullptr, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	//todo:clear this to black
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal_static void Win32DisplayBufferInWindow(HDC DeviceContext, Win32BitmapBuffer Buffer, int WindowWidth, int WindowHeight)
{
	//aspect ratio correction
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

LRESULT CALLBACK Win32WindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
		case WM_SIZE:
		{
			//auto Dimension = Win32GetWindowDimension(Window);
			//Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
		} break;

		case WM_DESTROY:
		{
			//Todo: recreate window
			GlobalRunning = false;
		} break;

		case WM_CLOSE:
		{
			//Todo: message user
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			u32 VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) != 0);

			if (VKCode == 'W')
			{

			}
			else if (VKCode == 'A')
			{

			}
			else if (VKCode == 'S')
			{

			}
			else if (VKCode == 'D')
			{

			}
			else if (VKCode == 'Q')
			{

			}
			else if (VKCode == 'E')
			{

			}
			else if (VKCode == VK_UP)
			{

			}
			else if (VKCode == VK_DOWN)
			{

			}
			else if (VKCode == VK_LEFT)
			{

			}
			else if (VKCode == VK_RIGHT)
			{

			}
			else if (VKCode == VK_ESCAPE)
			{

			}
			else if (VKCode == VK_SPACE)
			{

			}
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			auto Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, GlobalBackBuffer, Dimension.Width, Dimension.Height);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return Result;
}

int Engine::Run(HINSTANCE Instance)
{
	printf("test");

	Engine::Log::LogCore(Engine::Log::Level::Warn, "This is a warn!");
	Engine::Log::Log(Engine::Log::Level::Info, "This is a client!");

	//MessageBox(0, "Hello!", "Engine", MB_OK | MB_ICONINFORMATION);

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WNDCLASS WindowClass = {};
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32WindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "GameEngineClass";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Game Engine",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			nullptr,
			nullptr,
			Instance,
			nullptr
		);

		if (Window)
		{
			int XOffset = 0;
			int YOffset = 0;

			MSG Message;

			GlobalRunning = true;
			while (GlobalRunning)
			{
				while (PeekMessageA(&Message, nullptr, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						i16 StickX = Pad->sThumbLX;
						i16 StickY = Pad->sThumbLY;

						if (AButton)
						{
							++XOffset;
						}
						if (BButton)
						{
							++YOffset;
						}
					}
					else
					{
						//Note: controller not available
					}
				}

				//Rumble
				/*XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);*/

				RenderGradient(GlobalBackBuffer, XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);


				auto Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, GlobalBackBuffer, Dimension.Width, Dimension.Height);

				ReleaseDC(Window, DeviceContext);
			}
		}
		else
		{
			//todo: log
		}
	}
	else
	{
		//todo: log
	}

	return 0;
}