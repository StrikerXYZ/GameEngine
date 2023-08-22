// GameEngine.cpp : Defines the entry point for the application.
//

#include "GameEngine.h"

#include <windows.h>
#include <stdint.h>

#define local_static static
#define global_static static //variables cannot be used in other translation units and initialized to 0 by default
#define internal_static static

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

global_static bool Running;

global_static BITMAPINFO BitmapInfo;
global_static void* BitmapMemory;
global_static int BitmapWidth;
global_static int BitmapHeight;
global_static int BytesPerPixel = 4; //Need to be aligned to boundary for speed

internal_static void RenderGradient(int XOffset, int YOffset)
{
	int Width = BitmapWidth;
	int Height = BitmapHeight;
	int Pitch = Width * BytesPerPixel;
	u8* Row = static_cast<u8*>(BitmapMemory);
	for (int Y = 0; Y < BitmapHeight; ++Y)
	{
		u32* Pixel = reinterpret_cast<u32*>(Row);
		for (int X = 0; X < BitmapWidth; ++X) 
		{
			//Memory:		BB GG RR 00 - Little Endian
			//Registers:	xx RR GG BB
			u8 Blue = (X + XOffset);
			u8 Green = (Y + YOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}

		Row += Pitch;
	}
}

internal_static void Win32ResizeDIBSection(int Width, int Height) //DIB = Device Independant Bitmap
{
	//todo: bullet proof this - maybe free after
	if (BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;

	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = BitmapWidth * BitmapHeight * BytesPerPixel;
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	//todo:clear this to black
}

internal_static void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;

	StretchDIBits(
		DeviceContext,
		0, 0, BitmapWidth, BitmapHeight,
		X, Y, WindowWidth, WindowHeight,
		BitmapMemory,
		&BitmapInfo,
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
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int Width = ClientRect.right - ClientRect.left;
			int Height = ClientRect.bottom - ClientRect.top;
			Win32ResizeDIBSection(Width, Height);
		} break;

		case WM_DESTROY:
		{
			//Todo: recreate window
			Running = false;
		} break;

		case WM_CLOSE:
		{
			//Todo: message user
			Running = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return Result;
}

int CALLBACK WinMain( HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	MessageBox(0, "Hello!", "Engine", MB_OK | MB_ICONINFORMATION);

	WNDCLASS WindowClass = {}; 
	WindowClass.lpfnWndProc = Win32WindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "GameEngineClass";
	
	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Game Engine",
			WS_OVERLAPPEDWINDOW|WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0
		);

		if (Window)
		{
			int XOffset = 0;
			int YOffset = 0;

			MSG Message;
			Running = true;
			while (Running)
			{
				while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderGradient(XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);


				RECT ClientRect;
				GetClientRect(Window, &ClientRect);
				int WindowWidth = ClientRect.right - ClientRect.left;
				int WindowHeight = ClientRect.bottom - ClientRect.top;

				Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);

				ReleaseDC(Window, DeviceContext);

				++XOffset;
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