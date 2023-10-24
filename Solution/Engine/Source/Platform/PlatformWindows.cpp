// GameEngine.cpp : Defines the entry point for the application.
//
#include "PlatformWindows.hpp"

#include "Source/Definition.hpp"
#include "Source/EntryPoint.hpp"

#include <Windows.h>
#include <Xinput.h>
#include <xaudio2.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <math.h>
#include <malloc.h>

/*
TODO: partial list of stuff todo
- Saved game locations
- handle to executable
- asset loading path
- threading
- raw input
- sleep
- cursor clip
- fullscreen
- cursor visibility
- querycancelautoplay
- activate app
- blit improvements
- hardware acceleration
- get keyboard layout
*/

struct Win32_BitmapBuffer
{
	BITMAPINFO info;
	void* memory;
	i32 width;
	i32 height;
	i32 pitch;
	i32 bytes_per_pixel;
};

struct Win32_WindowDimension
{
	i32 width;
	i32 height;
};

struct Win32_SoundOutput
{
	u32 sample_rate;
	u16 num_channels;
	u16 bytes_per_sample;

	i32 running_sample_index;
	i32 wave_period;
	f32 t_sine;
	i32 latency_sample_count;
};

global_static bool global_running;
global_static Win32_BitmapBuffer global_back_buffer;

global_static IXAudio2SourceVoice* global_source_voice;
global_static f32* global_audio_memory;

Engine::FileResult Engine::PlatformRead(char* file_name)
{
	Engine::FileResult result = {};

	HANDLE file_handle = CreateFile(
		file_name,
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		NULL,
		nullptr
	);

	if (file_handle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER file_size;
		if(GetFileSizeEx(file_handle, &file_size))
		{
			const u32 file_size_32 = SafeTruncate32(file_size.QuadPart);
			result.content = VirtualAlloc(nullptr, file_size_32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (result.content)
			{
				DWORD bytes_read;
				if (ReadFile(file_handle, result.content, file_size_32, &bytes_read, nullptr) && (bytes_read == file_size_32))
				{
					//read file successfully
					result.content_size = file_size_32;
				}
				else
				{
					Engine::PlatformFree(result);
					result.content = nullptr;
				}
			}
		}

		CloseHandle(file_handle);
	}

	return result;
}

b32 Engine::PlatformWrite(char* file_name, Engine::FileResult& file)
{
	b32 result = false;

	HANDLE file_handle = CreateFile(
		file_name,
		GENERIC_WRITE,
		NULL,
		nullptr,
		CREATE_ALWAYS,
		NULL,
		nullptr
	);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		DWORD bytes_written;
		if (WriteFile(file_handle, file.content, file.content_size, &bytes_written, nullptr))
		{
			//write file successfully
			result = (bytes_written == file.content_size);
		}

		CloseHandle(file_handle);
	}

	return result;
}

void Engine::PlatformFree(Engine::FileResult& file)
{
	VirtualFree(file.content, 0, MEM_RELEASE);
}

internal_static void Win32_SetupConsole()
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

internal_static Win32_WindowDimension Win32_GetWindowDimension(HWND window_handle)
{
	Win32_WindowDimension result;

	RECT client_rect;
	GetClientRect(window_handle, &client_rect);
	result.width = client_rect.right - client_rect.left;
	result.height = client_rect.bottom - client_rect.top;

	return result;
}

internal_static void Win32_InitAudio(Win32_SoundOutput sound_output)
{
	HRESULT hr;
	hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		Halt(); return;
	}

	IXAudio2* xaudio_2 = nullptr;
	if (FAILED(hr = XAudio2Create(&xaudio_2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
	{
		Halt(); return;
	}

	IXAudio2MasteringVoice* master_voice = nullptr;
	if (FAILED(hr = xaudio_2->CreateMasteringVoice(&master_voice)))
	{
		Halt(); return;
	}

	WAVEFORMATEX wave_format = {};
	wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wave_format.wBitsPerSample = sound_output.bytes_per_sample * 8;
	wave_format.nAvgBytesPerSec = static_cast<u32>(sound_output.sample_rate * sound_output.num_channels * sound_output.bytes_per_sample);
	wave_format.nChannels = sound_output.num_channels;
	wave_format.nBlockAlign = sound_output.num_channels * sound_output.bytes_per_sample;
	wave_format.nSamplesPerSec = sound_output.sample_rate;
	wave_format.cbSize = 0;

	global_source_voice = nullptr;
	if (FAILED(hr = xaudio_2->CreateSourceVoice(&global_source_voice, &wave_format)))
	{
		Halt(); return;
	}

	global_audio_memory = new f32[sound_output.sample_rate * sound_output.num_channels];
	//{
	//	f32 note_freq = 60; //A1
	//	f32 PI = 3.14159265;
	//	for (int i = 0; i < sample_rate * num_channels; ++i)
	//	{
	//		global_audio_memory[i] = sin(i * 2 * PI * note_freq / sample_rate);
	//		global_audio_memory[i + 1] = sin(i * 2 * PI * (note_freq + 2) / sample_rate);
	//	}
	//}
	
	{
		local_static f32 t_sine = 0;
		u16 tone_volume = 1;
		u32 tone_frequency = 128;
		u32 wave_period = sound_output.sample_rate / tone_frequency;
		f32 PI = 3.14159265f;

		f32* sample_out = global_audio_memory;
		for (u32 i = 0; i < sound_output.sample_rate; ++i)
		{
			f32 sine_value = sinf(t_sine);
			f32 sample_value = sine_value * tone_volume;
			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

			t_sine += 2.f * PI * 1.f / static_cast<f32>(wave_period);
		}
	}

	XAUDIO2_BUFFER audio_buffer = {};
	audio_buffer.AudioBytes = sound_output.sample_rate * sound_output.num_channels * sound_output.bytes_per_sample;
	audio_buffer.Flags = 0;
	audio_buffer.PlayBegin = 0;
	audio_buffer.PlayLength = 0;
	audio_buffer.LoopBegin = 0;
	audio_buffer.LoopLength = 0;
	audio_buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	audio_buffer.pContext = nullptr;
	audio_buffer.pAudioData = reinterpret_cast<BYTE*>(global_audio_memory);

	if (FAILED(hr = global_source_voice->SubmitSourceBuffer(&audio_buffer)))
	{
		Halt(); return;
	}

	if (FAILED(hr = global_source_voice->Start()))
	{
		Halt(); return;
	}
}

internal_static void Win32_FillAudioBuffer(u32 sample_rate, u16 num_channels, f32* source_buffer)
{
	XAUDIO2_VOICE_STATE voice_state;
	global_source_voice->GetState(&voice_state);

	u32 num_memory = sample_rate * num_channels;
	u64 audio_pos = voice_state.SamplesPlayed;

	for (u32 i = 0; i < num_memory; ++i)
	{
		//global_audio_memory[i] = source_buffer[i];
	}
}

internal_static void Win32_ResizeDIBSection(Win32_BitmapBuffer* bitmap_buffer, int width, int height) //DIB = Device Independant Bitmap
{
	//todo: bullet proof this - maybe free after
	if (bitmap_buffer->memory)
	{
		VirtualFree(bitmap_buffer->memory, 0, MEM_RELEASE);
	}

	bitmap_buffer->width = width;
	bitmap_buffer->height = height;
	bitmap_buffer->bytes_per_pixel = 4; //Need to be aligned to boundary for speed

	bitmap_buffer->info.bmiHeader.biSize = sizeof(bitmap_buffer->info.bmiHeader);
	bitmap_buffer->info.bmiHeader.biWidth = bitmap_buffer->width;
	bitmap_buffer->info.bmiHeader.biHeight = -bitmap_buffer->height; //treat bitmap as top down
	bitmap_buffer->info.bmiHeader.biPlanes = 1;
	bitmap_buffer->info.bmiHeader.biBitCount = 32;
	bitmap_buffer->info.bmiHeader.biCompression = BI_RGB;

	u64 bitmap_memory_size = static_cast<u64>(bitmap_buffer->width * bitmap_buffer->height * bitmap_buffer->bytes_per_pixel);
	bitmap_buffer->memory = VirtualAlloc(nullptr, bitmap_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	//todo:clear this to black
	bitmap_buffer->pitch = width * bitmap_buffer->bytes_per_pixel;
}

internal_static void Win32_DisplayBufferInWindow(HDC device_context, Win32_BitmapBuffer bitmap_buffer, int window_width, int window_height)
{
	//aspect ratio correction
	StretchDIBits(
		device_context,
		0, 0, window_width, window_height,
		0, 0, bitmap_buffer.width, bitmap_buffer.height,
		bitmap_buffer.memory,
		&bitmap_buffer.info,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

internal_static void Win32_ProcessKeyboardMessage(Engine::GameButtonState& new_state, b32 is_down)
{
	Assert(new_state.is_ended_down != is_down);
	new_state.is_ended_down = is_down;
	new_state.num_half_transitions += 1;
}

internal_static void Win32_ProcessXInputDigitalButton(const Engine::GameButtonState& old_button, DWORD x_input_button_state, DWORD button_bit, Engine::GameButtonState& new_button)
{
	new_button.is_ended_down = (x_input_button_state & button_bit) == button_bit;
	new_button.num_half_transitions = (old_button.is_ended_down != new_button.is_ended_down);
}

internal_static void Win32_ProcessPendingMessages(Engine::GameControllerInput& keyboard_controller)
{
	MSG message;

	while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
	{
		const auto w_param = message.wParam;
		const auto l_param = message.lParam;

		switch (message.message)
		{
			case WM_QUIT:
			{
				global_running = false;
			} break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				u64 vk_code = w_param;
				const b32 was_down = ((l_param & (1 << 30)) != 0);
				const b32 is_down = ((l_param & (1 << 31)) == 0);

				if (was_down != is_down)
				{
					if (vk_code == 'W')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.move_up, is_down);
					}
					else if (vk_code == 'A')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.move_left, is_down);
					}
					else if (vk_code == 'S')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.move_down, is_down);
					}
					else if (vk_code == 'D')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.move_right, is_down);
					}
					else if (vk_code == 'Q')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.shoulder_left, is_down);
					}
					else if (vk_code == 'E')
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.shoulder_right, is_down);
					}
					else if (vk_code == VK_UP)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.action_up, is_down);
					}
					else if (vk_code == VK_DOWN)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.action_down, is_down);
					}
					else if (vk_code == VK_LEFT)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.action_left, is_down);
					}
					else if (vk_code == VK_RIGHT)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.action_right, is_down);
					}
					else if (vk_code == VK_ESCAPE)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.back, is_down);
					}
					else if (vk_code == VK_RETURN)
					{
						Win32_ProcessKeyboardMessage(keyboard_controller.start, is_down);
					}

					bool AltDown = (l_param & (1u << 29)) != 0;
					if (vk_code == VK_F4 && AltDown)
					{
						global_running = false;
					}
				}
			} break;

			default:
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			} break;
		}
	}
}

internal_static f32 Win32_ProcessXInputStickValue(i16 value, i16 dead_zone)
{
	if (value < -dead_zone)
	{
		return value / 32768.f;
	}
	else if (value > dead_zone)
	{
		return value / 32767.f;
	}
	else
	{
		return 0.f;
	}
}

LRESULT CALLBACK Win32_WindowCallback(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
	LRESULT result = 0;

	switch (message)
	{
	case WM_SIZE:
	{
		//auto Dimension = Win32GetWindowDimension(Window);
		//Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
	} break;

	case WM_DESTROY:
	{
		//Todo: recreate window
		global_running = false;
	} break;

	case WM_CLOSE:
	{
		//Todo: message user
		global_running = false;
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
		Halt(); //This should never get called
	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC device_context = BeginPaint(window_handle, &paint);
		auto dimension = Win32_GetWindowDimension(window_handle);
		Win32_DisplayBufferInWindow(device_context, global_back_buffer, dimension.width, dimension.height);
		EndPaint(window_handle, &paint);
	} break;

	default:
	{
		result = DefWindowProc(window_handle, message, w_param, l_param);
	} break;
	}

	return result;
}

int CALLBACK WinMain(HINSTANCE Instance, [[maybe_unused]] HINSTANCE PrevInstance, [[maybe_unused]] LPSTR CommandLine, [[maybe_unused]] int ShowCode)
{
	Win32_SetupConsole();

	Engine::PlatformLaunch();

	LARGE_INTEGER performance_frequency_result;
	QueryPerformanceFrequency(&performance_frequency_result);
	i64 performance_frequency = performance_frequency_result.QuadPart;

	//MessageBox(0, "Hello!", "Engine", MB_OK | MB_ICONINFORMATION);

	Win32_ResizeDIBSection(&global_back_buffer, 1280, 720);

	WNDCLASS window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = Win32_WindowCallback;
	window_class.hInstance = Instance;
	window_class.lpszClassName = "GameEngineClass";


	if (RegisterClass(&window_class))
	{
		HWND window_handle = CreateWindowEx(
			0,
			window_class.lpszClassName,
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

		if (window_handle)
		{
			Win32_SoundOutput sound_output = {};
			sound_output.sample_rate = 48000;
			sound_output.num_channels = 2;
			sound_output.bytes_per_sample = sizeof(float);
			Win32_InitAudio(sound_output);

			global_running = true;

			f32* audio_samples = static_cast<f32*>(VirtualAlloc(nullptr, sound_output.sample_rate * sound_output.num_channels * sizeof(f32), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

			LPVOID BaseAddress = nullptr;
			if constexpr (ENGINE_BUILD_DEBUG)
			{
				BaseAddress = reinterpret_cast<LPVOID>(TeraBytes(1));
			}

			Engine::GameMemory memory = {};
			memory.permanent_storage_size = MegaBytes(64);
			memory.transient_storage_size = GigaBytes(4);

			const auto total_size = memory.permanent_storage_size + memory.transient_storage_size;
			memory.permanent_storage = VirtualAlloc(BaseAddress, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			memory.transient_storage = static_cast<u8*>(memory.permanent_storage) + memory.permanent_storage_size;

			if (audio_samples && memory.permanent_storage && memory.transient_storage)
			{
				Engine::GameInput input[2] = {};
				Engine::GameInput& new_input = input[0];
				Engine::GameInput& old_input = input[1];

				LARGE_INTEGER last_counter;
				QueryPerformanceCounter(&last_counter);

				u64 last_cycle_count = __rdtsc();

				while (global_running)
				{
					Engine::GameControllerInput& old_keyboard_controller = get_controller(old_input, 0);
					Engine::GameControllerInput& new_keyboard_controller = get_controller(new_input, 0);
					new_keyboard_controller = {};
					new_keyboard_controller.is_connected = true;
					new_keyboard_controller.move_left.is_ended_down = old_keyboard_controller.move_left.is_ended_down;
					new_keyboard_controller.move_right.is_ended_down = old_keyboard_controller.move_right.is_ended_down;
					new_keyboard_controller.move_up.is_ended_down = old_keyboard_controller.move_up.is_ended_down;
					new_keyboard_controller.move_down.is_ended_down = old_keyboard_controller.move_down.is_ended_down;
					new_keyboard_controller.action_left.is_ended_down = old_keyboard_controller.action_left.is_ended_down;
					new_keyboard_controller.action_right.is_ended_down = old_keyboard_controller.action_right.is_ended_down;
					new_keyboard_controller.action_up.is_ended_down = old_keyboard_controller.action_up.is_ended_down;
					new_keyboard_controller.action_down.is_ended_down = old_keyboard_controller.action_down.is_ended_down;
					new_keyboard_controller.shoulder_left.is_ended_down = old_keyboard_controller.shoulder_left.is_ended_down;
					new_keyboard_controller.shoulder_right.is_ended_down = old_keyboard_controller.shoulder_right.is_ended_down;
					new_keyboard_controller.back.is_ended_down = old_keyboard_controller.back.is_ended_down;
					new_keyboard_controller.start.is_ended_down = old_keyboard_controller.start.is_ended_down;

					Win32_ProcessPendingMessages(new_keyboard_controller);

					DWORD num_max_controllers = XUSER_MAX_COUNT;
					if (num_max_controllers > ArrayCount(new_input.controllers) - 1)
					{
						num_max_controllers = ArrayCount(new_input.controllers) - 1;
					}
					for (DWORD controller_index = 0; controller_index < num_max_controllers; ++controller_index)
					{

						DWORD gamepad_controller_index = controller_index + 1;
						Engine::GameControllerInput& old_controller = get_controller(old_input, gamepad_controller_index);
						Engine::GameControllerInput& new_controller = get_controller(new_input, gamepad_controller_index);

						XINPUT_STATE controller_state;
						if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
						{
							new_controller.is_connected = true;

							XINPUT_GAMEPAD* pad = &controller_state.Gamepad;

							new_controller.is_analog = true;

							new_controller.stick_average_x = Win32_ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							new_controller.stick_average_y = Win32_ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);


							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								new_controller.stick_average_x = 1.f;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								new_controller.stick_average_x = -1.f;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								new_controller.stick_average_y = -1.f;
							}
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								new_controller.stick_average_y = 1.f;
							}

							f32 threshold = 0.5f;
							Win32_ProcessXInputDigitalButton(old_controller.move_up, (new_controller.stick_average_y < -threshold), 1, new_controller.move_up);
							Win32_ProcessXInputDigitalButton(old_controller.move_down, (new_controller.stick_average_y > threshold), 1, new_controller.move_down);
							Win32_ProcessXInputDigitalButton(old_controller.move_left, (new_controller.stick_average_x < -threshold), 1, new_controller.move_left);
							Win32_ProcessXInputDigitalButton(old_controller.move_right, (new_controller.stick_average_x > threshold), 1, new_controller.move_right);

							Win32_ProcessXInputDigitalButton(old_controller.action_up, pad->wButtons, XINPUT_GAMEPAD_Y, new_controller.action_up);
							Win32_ProcessXInputDigitalButton(old_controller.action_down, pad->wButtons, XINPUT_GAMEPAD_A, new_controller.action_down);
							Win32_ProcessXInputDigitalButton(old_controller.action_left, pad->wButtons, XINPUT_GAMEPAD_X, new_controller.action_left);
							Win32_ProcessXInputDigitalButton(old_controller.action_right, pad->wButtons, XINPUT_GAMEPAD_B, new_controller.action_right);

							Win32_ProcessXInputDigitalButton(old_controller.shoulder_left, pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, new_controller.shoulder_left);
							Win32_ProcessXInputDigitalButton(old_controller.shoulder_right, pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, new_controller.shoulder_right);

							Win32_ProcessXInputDigitalButton(old_controller.start, pad->wButtons, XINPUT_GAMEPAD_START, new_controller.start);
							Win32_ProcessXInputDigitalButton(old_controller.back, pad->wButtons, XINPUT_GAMEPAD_BACK, new_controller.back);

						}
						else
						{
							new_controller.is_connected = false;
						}
					}

					//Rumble
					/*XINPUT_VIBRATION Vibration;
					Vibration.wLeftMotorSpeed = 60000;
					Vibration.wRightMotorSpeed = 60000;
					XInputSetState(0, &Vibration);*/

					Engine::GameSoundBuffer sound_buffer = {};
					sound_buffer.samples_per_second = sound_output.sample_rate;
					sound_buffer.sample_count = sound_output.sample_rate;
					sound_buffer.samples = audio_samples;

					Engine::GameOffscreenBuffer buffer = {};
					buffer.memory = global_back_buffer.memory;
					buffer.width = global_back_buffer.width;
					buffer.height = global_back_buffer.height;
					buffer.pitch = global_back_buffer.pitch;

					Engine::PlatformLoop(&memory, input, buffer, sound_buffer);

					Win32_FillAudioBuffer(sound_output.sample_rate, sound_output.num_channels, sound_buffer.samples);


					/*XAUDIO2_VOICE_STATE state;
					GlobalSourceVoice->GetState(&state);
					DWORD PlayCursor = state.SamplesPlayed;
					DWORD WriteCursor = */

					//GlobalAudioBuffer.Loc
					////play sound
					//f32 PI = 3.14159;
					//for (int i = 0; i < 44100; ++i)
					//{
					//	f32 sample = sinf(i * 2 * PI / 44100);
					//	GlobalAudioBufferMemory[i] = sample;
					//}

					HDC device_context = GetDC(window_handle);
					auto dimension = Win32_GetWindowDimension(window_handle);
					Win32_DisplayBufferInWindow(device_context, global_back_buffer, dimension.width, dimension.height);
					ReleaseDC(window_handle, device_context);

					u64 end_cycle_count = __rdtsc();

					LARGE_INTEGER end_counter;
					QueryPerformanceCounter(&end_counter);

					u64 cycles_elapsed = end_cycle_count - last_cycle_count;
					i64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
					f64 ms_per_frame = (1000. * static_cast<f64>(counter_elapsed)) / static_cast<f64>(performance_frequency);
					f64 fps = static_cast<f64>(performance_frequency) / static_cast<f64>(counter_elapsed);
					f64 mega_cycles_per_frame = static_cast<f64>(cycles_elapsed) / (1000. * 1000.);

					/*char write_buffer[256];
					sprintf(write_buffer, "ms/frame: %.2fms, %.2ffps %.2fc\n", ms_per_frame, fps, mega_cycles_per_frame);
					Engine::Log::LogCore(Engine::Log::Level::Warn, write_buffer);*/

					last_counter = end_counter;
					last_cycle_count = end_cycle_count;

					Swap(old_input, new_input);
				}
			}
			else
			{
				//todo:log
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