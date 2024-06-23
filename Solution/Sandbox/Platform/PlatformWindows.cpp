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
	r32 t_sine;
	i32 latency_sample_count;
};

struct Win32_GameCode
{
	HMODULE game_code_dll;
	FILETIME dll_last_write_time;
	FuncPlatformLoop* Loop;
	FuncPlatformGetSoundSamples* GetSoundSamples;
	b32 is_valid;
};

constexpr auto WinPathNameCount = MAX_PATH;

struct Win32_ReplayBuffer
{
	HANDLE file_handle;
	HANDLE memory_map;
	char filename[WinPathNameCount];
	void* memory_block;
};

struct Win32_State
{
	u64 memory_size;
	void* memory_block;
	Win32_ReplayBuffer replay_buffers[4];

	HANDLE recording_handle;
	int input_recording_index = 0;

	HANDLE playback_handle;
	int input_playing_index = 0;

	char exe_filepath[WinPathNameCount];
	char* exe_filename;
};


global_static bool global_running;
global_static bool global_paused;
global_static Win32_BitmapBuffer global_back_buffer;

global_static IXAudio2SourceVoice* global_source_voice;
global_static r32* global_audio_memory;

global_static i64 global_performance_frequency;


extern "C" 
ENGINE_API PLATFORM_READ_FILE(PlatformReadDefinition)
{
	FileResult result = {};

	HANDLE file_handle = CreateFileA(
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
					//PlatformFree(result);
					//result.content = nullptr;
					Halt();
				}
			}
		}

		CloseHandle(file_handle);
	}

	return result;
}

extern "C" 
ENGINE_API PLATFORM_WRITE_FILE(PlatformWriteDefinition)
{
	b32 result = false;

	HANDLE file_handle = CreateFileA(
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

extern "C" 
ENGINE_API PLATFORM_FREE_FILE(PlatformFreeDefinition)
{
	VirtualFree(file.content, 0, MEM_RELEASE);
}


internal_static void ConcatStrings(u64 source_a_count, char* source_a, u64 source_b_count, char* source_b, u64 dest_count, char* dest)
{
	for (u64 index = 0; index < source_a_count; ++index)
	{
		*dest++ = *source_a++;
	}

	for (u64 index = 0; index < source_b_count; ++index)
	{
		*dest++ = *source_b++;
	}

	*dest++ = 0;
}

internal_static u64 StringLength(char* file_name)
{
	u64 length = 0;
	while (*file_name++)
	{
		++length;
	}

	return length;
}


internal_static void Win32_BuildEXEFilepath(Win32_State& state, char* file_name, u64 dest_count, char* dest)
{
	ConcatStrings(static_cast<u64>(state.exe_filename - state.exe_filepath), state.exe_filepath,
		StringLength(file_name), file_name,
		dest_count, dest);
}

internal_static void Win32_GetInputFileLocation(Win32_State& state, b32 input_stream, int slot_index, u64 dest_count, char* dest)
{
	char temp[64];
	wsprintf(temp, "input_%d_%s.rec", slot_index, input_stream? "input" : "state");
	Win32_BuildEXEFilepath(state, temp, dest_count, dest);
}

internal_static Win32_ReplayBuffer& Win32_GetReplayBuffer(Win32_State& state, int recording_index)
{
	Assert(static_cast<u64>(recording_index) < ArrayCount(state.replay_buffers));
	return state.replay_buffers[recording_index];
}

internal_static void Win32_BeginRecordingInput(Win32_State& state, int input_recording_index)
{
	auto& replay_buffer = Win32_GetReplayBuffer(state, input_recording_index);
	if (replay_buffer.memory_block)
	{
		state.input_recording_index = input_recording_index;

		char filename[WinPathNameCount];
		Win32_GetInputFileLocation(state, true, input_recording_index, sizeof(filename), filename);
		state.recording_handle = CreateFileA(filename, GENERIC_WRITE, NULL, nullptr, CREATE_ALWAYS, NULL, nullptr);

		CopyMemory(replay_buffer.memory_block, state.memory_block, state.memory_size);
	}
}

internal_static void Win32_EndRecordingInput(Win32_State& state)
{
	CloseHandle(state.recording_handle);
	state.input_recording_index = 0;
}

internal_static void Win32_BeginPlaybackInput(Win32_State& state, int input_playing_index)
{
	auto& replay_buffer = Win32_GetReplayBuffer(state, input_playing_index);
	if (replay_buffer.memory_block)
	{
		state.input_playing_index = input_playing_index;

		char filename[WinPathNameCount];
		Win32_GetInputFileLocation(state, true, input_playing_index, sizeof(filename), filename);
		state.playback_handle = CreateFileA(filename, GENERIC_READ, NULL, nullptr, OPEN_EXISTING, NULL, nullptr);

		CopyMemory(state.memory_block, replay_buffer.memory_block, state.memory_size);
	}
}

internal_static void Win32_EndPlaybackInput(Win32_State& state)
{
	CloseHandle(state.playback_handle);
	state.input_playing_index = 0;
}

internal_static void Win32_RecordInput(Win32_State& state, GameInput& input)
{
	DWORD bytes_written;
	WriteFile(state.recording_handle, &input, sizeof(input), &bytes_written, nullptr);
}

internal_static void Win32_PlayBackInput(Win32_State& state, GameInput& input)
{
	DWORD bytes_read;
	if (ReadFile(state.playback_handle, &input, sizeof(input), &bytes_read, nullptr))
	{
		if(bytes_read == 0)
		{
			int playing_index = state.input_playing_index;
			Win32_EndPlaybackInput(state);
			Win32_BeginPlaybackInput(state, playing_index);
			ReadFile(state.playback_handle, &input, sizeof(input), &bytes_read, nullptr);
		}
	}
	
}

internal_static FILETIME Win32_GetFileLastWriteTime(char* filename)
{
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	if (GetFileAttributesEx(filename, GetFileExInfoStandard, &file_data))
	{
		return file_data.ftLastWriteTime;
	}

	return {};
}

internal_static Win32_GameCode Win32_LoadGameCode(char* source_dll, char* temp_dll)
{
	Win32_GameCode result = {};
	
	result.dll_last_write_time = Win32_GetFileLastWriteTime(source_dll);

	CopyFile(source_dll, temp_dll, FALSE);
	result.game_code_dll = LoadLibraryA(temp_dll);
	if (result.game_code_dll)
	{
		result.Loop = reinterpret_cast<FuncPlatformLoop*>(GetProcAddress(result.game_code_dll, "PlatformLoop"));
		result.GetSoundSamples = reinterpret_cast<FuncPlatformGetSoundSamples*>(GetProcAddress(result.game_code_dll, "PlatformGetSoundSamples"));
		result.is_valid = result.Loop;
	}

	if (!result.is_valid)
	{
		result.Loop = nullptr;
		result.GetSoundSamples = nullptr;
	}

	return result;
}

internal_static void Win32_UnloadGameCode(Win32_GameCode& game_code)
{
	if (game_code.game_code_dll)
	{
		FreeLibrary(game_code.game_code_dll);
	}

	game_code.is_valid = false;
	game_code.Loop = nullptr;
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

	global_audio_memory = new r32[sound_output.sample_rate * sound_output.num_channels];
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
		local_static r32 t_sine = 0;
		u16 tone_volume = 1;
		u32 tone_frequency = 128;
		u32 wave_period = sound_output.sample_rate / tone_frequency;
		r32 pi = 3.14159265f;

		r32* sample_out = global_audio_memory;
		for (u32 i = 0; i < sound_output.sample_rate; ++i)
		{
			r32 sine_value = sinf(t_sine);
			r32 sample_value = sine_value * tone_volume;
			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

			t_sine += 2.f * pi * 1.f / static_cast<r32>(wave_period);
		}
	}

#if 0
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
#endif
}

internal_static void Win32_FillAudioBuffer(u32 sample_rate, u16 num_channels, r32* source_buffer)
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
	i32 offset_x = 10;
	i32 offset_y = 10;

	//Clear screen
	PatBlt(device_context, 0, 0, window_width, offset_y, BLACKNESS);
	PatBlt(device_context, 0, offset_y + bitmap_buffer.height, window_width, window_height, BLACKNESS);
	PatBlt(device_context, 0, 0, offset_x, window_height, BLACKNESS);
	PatBlt(device_context, offset_x + bitmap_buffer.width, 0, window_width, window_height, BLACKNESS);


	//aspect ratio correction
	StretchDIBits(
		device_context,
		offset_x, offset_y, bitmap_buffer.width, bitmap_buffer.height, //For shipping: window_width, window_height
		0, 0, bitmap_buffer.width, bitmap_buffer.height,
		bitmap_buffer.memory,
		&bitmap_buffer.info,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

internal_static void Win32_ProcessKeyboardMessage(GameButtonState& new_state, b32 is_down)
{
	if (new_state.is_ended_down != is_down)
	{
		new_state.is_ended_down = is_down;
		new_state.num_half_transitions += 1;
	}
}

internal_static void Win32_ProcessXInputDigitalButton(const GameButtonState& old_button, DWORD x_input_button_state, DWORD button_bit, GameButtonState& new_button)
{
	new_button.is_ended_down = (x_input_button_state & button_bit) == button_bit;
	new_button.num_half_transitions = (old_button.is_ended_down != new_button.is_ended_down);
}

internal_static void Win32_ProcessPendingMessages(Win32_State& state, GameControllerInput& keyboard_controller)
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
					if (!global_paused)
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
						else if (vk_code == 'L')
						{
							if (is_down)
							{
								if (state.input_playing_index == 0)
								{
									if (state.input_recording_index == 0)
									{
										Win32_BeginRecordingInput(state, 1);
									}
									else
									{
										Win32_EndRecordingInput(state);
										Win32_BeginPlaybackInput(state, 1);
									}
								}
								else
								{
									Win32_EndPlaybackInput(state);
								}
							}
						}
					}
					
					if (vk_code == 'P')
					{
						if (is_down)
						{
							global_paused = !global_paused;
						}
					}

					bool alt_down = (l_param & (1u << 29)) != 0;
					if (vk_code == VK_F4 && alt_down)
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

internal_static r32 Win32_ProcessXInputStickValue(i16 value, i16 dead_zone)
{
	if (value < -dead_zone)
	{
		return (value + dead_zone) / (32768.f - dead_zone);
	}
	else if (value > dead_zone)
	{
		return (value + dead_zone) / (32767.f - dead_zone);
	}
	else
	{
		return 0.f;
	}
}

internal_static MONITORINFOEX Win32_GetMonitor(HWND window_handle)
{
	HMONITOR monitor = MonitorFromWindow(window_handle, MONITOR_DEFAULTTOPRIMARY);

	MONITORINFOEX monitor_info;
	monitor_info.cbSize = sizeof(MONITORINFOEX);

	if (GetMonitorInfo(monitor, &monitor_info)) {
		return monitor_info;
	}

	Halt();
	return {};
}

internal_static DEVMODE Win32_GetDevMode(LPCSTR device_name)
{
	DEVMODE dev_mode;
	dev_mode.dmSize = sizeof(dev_mode);

	if (EnumDisplaySettings(device_name, ENUM_CURRENT_SETTINGS, &dev_mode) != 0) {
		return dev_mode;//dm.dmDisplayFrequency;
	}

	Halt();
	return {};
}

internal_static void Win32_GetEXEFilename(Win32_State& state)
{
	DWORD size_of_filename = GetModuleFileNameA(nullptr, state.exe_filepath, sizeof(state.exe_filepath));
	state.exe_filename = state.exe_filepath;
	for (char* scan = state.exe_filepath; *scan; ++scan)
	{
		if (*scan == '\\')
		{
			state.exe_filename = scan + 1;
		}
	}
}

inline LARGE_INTEGER Win32_GetWallClock()
{
	LARGE_INTEGER performance_counter;
	QueryPerformanceCounter(&performance_counter);
	return performance_counter;
}

inline r64 Win32_GetSecondElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	return static_cast<r64>(end.QuadPart - start.QuadPart) / static_cast<r64>(global_performance_frequency);
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
		if (w_param == TRUE)
		{
			SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 255, LWA_ALPHA);
		}
		else
		{
			SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 64, LWA_ALPHA);
		}
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

	Win32_State state{};

	//PlatformLaunch();

	LARGE_INTEGER performance_frequency_result;
	QueryPerformanceFrequency(&performance_frequency_result);
	global_performance_frequency = performance_frequency_result.QuadPart;

	//Set windows schedular granularity to 1ms
	//So the sleep can be more granular
	UINT desired_schedular_ms = 1;
	b32 is_granular_sleep = (timeBeginPeriod(desired_schedular_ms) == TIMERR_NOERROR);

	//Target Display Size = 1920 x 1080
	// 
	// Width:
	// 1920 -> 2048 (pot) = 2048 - 1920 = 128 pixels
	// 
	// Height:
	// 1080 -> 2048 (pot) = 2048 - 1080 = 968 pixels
	// 1024 + 128 = 1152 
	Win32_ResizeDIBSection(&global_back_buffer, 960, 540);


	Win32_GetEXEFilename(state);
	char source_game_code_dll_path[WinPathNameCount];
	Win32_BuildEXEFilepath(state, "Engine.dll", sizeof(source_game_code_dll_path), source_game_code_dll_path);
	char temp_game_code_dll_path[WinPathNameCount];
	Win32_BuildEXEFilepath(state, "Engine_Temp.dll", sizeof(temp_game_code_dll_path), temp_game_code_dll_path);


	WNDCLASS window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = Win32_WindowCallback;
	window_class.hInstance = Instance;
	window_class.lpszClassName = "GameEngineClass";

	if (RegisterClass(&window_class))
	{
		HWND window_handle = CreateWindowEx(
			NULL, //WS_EX_TOPMOST | WS_EX_LAYERED,
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
			HDC device_constext = GetDC(window_handle);
			i32 monitor_refresh_hz = 60;//Win32_GetDevMode(Win32_GetMonitor(window_handle).szDevice).dmDisplayFrequency;
			i32 win32_refresh_hz = GetDeviceCaps(device_constext, VREFRESH);
			if (win32_refresh_hz > 1)
			{
				monitor_refresh_hz = win32_refresh_hz;
			}
			ReleaseDC(window_handle, device_constext);

			r64 game_update_hz = static_cast<r64>(monitor_refresh_hz) / 2.;
			r64 target_seconds_per_frame = 1. / static_cast<r64>(game_update_hz);

			Win32_SoundOutput sound_output = {};
			sound_output.sample_rate = 48000;
			sound_output.num_channels = 2;
			sound_output.bytes_per_sample = sizeof(float);
			Win32_InitAudio(sound_output);

			global_running = true;
			global_paused = false;

			r32* audio_samples = static_cast<r32*>(VirtualAlloc(nullptr, sound_output.sample_rate * sound_output.num_channels * sizeof(r32), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

			LPVOID base_address = nullptr;
			if constexpr (ENGINE_BUILD_DEBUG)
			{
				base_address = nullptr;//reinterpret_cast<LPVOID>(TeraBytes(1));
			}

			ThreadContext thread {};

			GameMemory memory = {};
			memory.permanent_storage_size = MegaBytes(64);
			memory.transient_storage_size = GigaBytes(1);
			memory.Debug_PlatformRead = PlatformReadDefinition;
			memory.Debug_PlatformWrite = PlatformWriteDefinition;
			memory.Debug_PlatformFree = PlatformFreeDefinition;

			state.memory_size = memory.permanent_storage_size + memory.transient_storage_size;
			state.memory_block = VirtualAlloc(base_address, state.memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			memory.permanent_storage = state.memory_block;
			memory.transient_storage = static_cast<u8*>(memory.permanent_storage) + memory.permanent_storage_size;

			for (int replay_index = 0; replay_index < static_cast<int>(ArrayCount(state.replay_buffers)); ++replay_index)
			{
				Win32_ReplayBuffer& replay_buffer = state.replay_buffers[replay_index];

				Win32_GetInputFileLocation(state, false, replay_index, sizeof(replay_buffer.filename), replay_buffer.filename);
				replay_buffer.file_handle = CreateFileA(replay_buffer.filename, GENERIC_READ | GENERIC_WRITE, NULL, nullptr, CREATE_ALWAYS, NULL, nullptr);
				
				LARGE_INTEGER max_size;
				max_size.QuadPart = static_cast<LONGLONG>(state.memory_size);
				replay_buffer.memory_map = CreateFileMappingA(replay_buffer.file_handle, nullptr, PAGE_READWRITE, 
					static_cast<u32>(max_size.HighPart), static_cast<u32>(max_size.LowPart), nullptr);
				replay_buffer.memory_block = MapViewOfFile(replay_buffer.memory_map, FILE_MAP_ALL_ACCESS, 0, 0, state.memory_size);

				if (replay_buffer.memory_block)
				{

				}
				else
				{
					Halt();
				}
			}

			if (audio_samples && memory.permanent_storage && memory.transient_storage)
			{
				GameInput input[2] = {};
				GameInput& new_input = input[0];
				GameInput& old_input = input[1];

				LARGE_INTEGER last_counter = Win32_GetWallClock();

				auto game_code = Win32_LoadGameCode(source_game_code_dll_path, temp_game_code_dll_path);

				u64 last_cycle_count = __rdtsc();

				while (global_running)
				{
					new_input.frame_delta = static_cast<r32>(target_seconds_per_frame);

					FILETIME check_file_time = Win32_GetFileLastWriteTime(source_game_code_dll_path);
					if (CompareFileTime(&game_code.dll_last_write_time, &check_file_time) != 0)
					{
						Win32_UnloadGameCode(game_code);
						game_code = Win32_LoadGameCode(source_game_code_dll_path, temp_game_code_dll_path);
					}

					GameControllerInput& old_keyboard_controller = get_controller(old_input, 0);
					GameControllerInput& new_keyboard_controller = get_controller(new_input, 0);
					new_keyboard_controller = {};
					new_keyboard_controller.is_connected = true;
					for (int index = 0; index < 12; ++index)
					{
						new_keyboard_controller.buttons[index].is_ended_down = old_keyboard_controller.buttons[index].is_ended_down;
					}

					Win32_ProcessPendingMessages(state, new_keyboard_controller);
					if (!global_paused)
					{
						POINT mouse_point;
						GetCursorPos(&mouse_point);
						ScreenToClient(window_handle, &mouse_point);
						new_input.mouse_x = mouse_point.x;
						new_input.mouse_y = mouse_point.y;
						new_input.mouse_z = 0; //todo: support mouse wheel
						Win32_ProcessKeyboardMessage(new_input.mouse_buttons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
						Win32_ProcessKeyboardMessage(new_input.mouse_buttons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
						Win32_ProcessKeyboardMessage(new_input.mouse_buttons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
						Win32_ProcessKeyboardMessage(new_input.mouse_buttons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32_ProcessKeyboardMessage(new_input.mouse_buttons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

						DWORD num_max_controllers = XUSER_MAX_COUNT;
						if (num_max_controllers > ArrayCount(new_input.controllers) - 1)
						{
							num_max_controllers = ArrayCount(new_input.controllers) - 1;
						}
						for (DWORD controller_index = 0; controller_index < num_max_controllers; ++controller_index)
						{

							DWORD gamepad_controller_index = controller_index + 1;
							GameControllerInput& old_controller = get_controller(old_input, gamepad_controller_index);
							GameControllerInput& new_controller = get_controller(new_input, gamepad_controller_index);

							XINPUT_STATE controller_state;
							if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
							{
								new_controller.is_connected = true;
								new_controller.is_analog = old_controller.is_analog;

								XINPUT_GAMEPAD* pad = &controller_state.Gamepad;

								new_controller.stick_average_x = Win32_ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								new_controller.stick_average_y = Win32_ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									new_controller.stick_average_x = 1.f;
									new_controller.is_analog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									new_controller.stick_average_x = -1.f;
									new_controller.is_analog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									new_controller.stick_average_y = -1.f;
									new_controller.is_analog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									new_controller.stick_average_y = 1.f;
									new_controller.is_analog = false;
								}

								if (new_controller.stick_average_x != 0.f || new_controller.stick_average_y != 0.f)
								{
									new_controller.is_analog = true;
								}

								r32 threshold = 0.5f;
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

						GameSoundBuffer sound_buffer = {};
						sound_buffer.samples_per_second = sound_output.sample_rate;
						sound_buffer.sample_count = sound_output.sample_rate;
						sound_buffer.samples = audio_samples;

						GameOffscreenBuffer buffer = {};
						buffer.memory = global_back_buffer.memory;
						buffer.width = global_back_buffer.width;
						buffer.height = global_back_buffer.height;
						buffer.pitch = global_back_buffer.pitch;
						buffer.bytes_per_pixel = global_back_buffer.bytes_per_pixel;

						if (state.input_recording_index)
						{
							Win32_RecordInput(state, new_input);
						}

						if (state.input_playing_index)
						{
							Win32_PlayBackInput(state, new_input);
						}

						if (game_code.Loop)
						{
							game_code.Loop(thread, &memory, input, buffer);
						}

						if (game_code.GetSoundSamples)
						{
							game_code.GetSoundSamples(thread, &memory, sound_buffer);
						}

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

						LARGE_INTEGER work_counter = Win32_GetWallClock();
						i64 counter_elapsed = work_counter.QuadPart - last_counter.QuadPart;
						const r64 ms_per_frame = (1000. * static_cast<r64>(counter_elapsed)) / static_cast<r64>(global_performance_frequency);
						r64 fps = static_cast<r64>(global_performance_frequency) / static_cast<r64>(counter_elapsed);

						r64 work_seconds_elapsed = Win32_GetSecondElapsed(last_counter, work_counter);

						r64 frame_seconds_elapsed = work_seconds_elapsed;
						if (frame_seconds_elapsed < target_seconds_per_frame)
						{
							if (is_granular_sleep)
							{
								DWORD sleep_ms = static_cast<DWORD>((target_seconds_per_frame - frame_seconds_elapsed) * 1000);
								if (sleep_ms > 0)
								{
									Sleep(sleep_ms);
								}
							}

							while (frame_seconds_elapsed < target_seconds_per_frame)
							{
								frame_seconds_elapsed = Win32_GetSecondElapsed(last_counter, Win32_GetWallClock());
							}
						}
						else
						{
							//missed frame rate!
						}

						LARGE_INTEGER end_counter = Win32_GetWallClock();
						last_counter = end_counter;

						Swap(old_input, new_input);

						{ //cycle per frame
							u64 end_cycle_count = __rdtsc();
							u64 cycles_elapsed = end_cycle_count - last_cycle_count;
							r64 mega_cycles_per_frame = static_cast<r64>(cycles_elapsed) / (1000. * 1000.);
							last_cycle_count = end_cycle_count;

							char write_buffer[256];
							sprintf(write_buffer, "ms/frame: %.2fms, %.2ffps %.2fc\n", ms_per_frame, fps, mega_cycles_per_frame);
							//Log::LogCore(Log::Level::Warn, write_buffer);
						}

					}
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