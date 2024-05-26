#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"


internal_static void GameOutputSound(const GameSoundBuffer& buffer, u32 tone_frequency)
{
	local_static r32 t_sine;
	i16 tone_volume = 1;
	r32 wave_period = static_cast<r32>(buffer.samples_per_second / tone_frequency);
	r32 pi = 3.14159265f;
	
	r32* sample_out = buffer.samples;
	for (u32 i = 0; i < buffer.sample_count; ++i)
	{
		r32 sine_value = 0;//sinf(t_sine);
		r32 sample_value = sine_value * tone_volume;
		*sample_out++ = sample_value;
		*sample_out++ = sample_value;

		t_sine += 2.f * pi * 1.f / wave_period;
	}
}

internal_static void RenderGradient(const GameOffscreenBuffer& bitmap_buffer, i32 x_offset, i32 y_offset)
{
	//todo: test buffer pass by value performance
	u8* row = static_cast<u8*>(bitmap_buffer.memory);
	for (int y = 0; y < bitmap_buffer.height; ++y)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < bitmap_buffer.width; ++x)
		{
			//Memory:		BB GG RR 00 - Little Endian
			//Registers:	xx RR GG BB
			u8 blue = static_cast<u8>(x + x_offset);
			u8 green = static_cast<u8>(y + y_offset);
			*pixel++ = static_cast<u32>((green << 16) | blue);
		}

		row += bitmap_buffer.pitch;
	}
}

internal_static i32 RoundToInt32(r32 real)
{
	return static_cast<i32>(real + .5f);
}

internal_static void DrawRectangle(const GameOffscreenBuffer& buffer, 
	r32 min_x, r32 min_y, r32 max_x, r32 max_y,
	u32 color
)
{
	i32 imin_x = RoundToInt32(min_x);
	i32 imin_y = RoundToInt32(min_y);
	i32 imax_x = RoundToInt32(max_x);
	i32 imax_y = RoundToInt32(max_y);

	if (imin_x < 0)
	{
		imin_x = 0;
	}
	if (imin_y < 0)
	{
		imin_y = 0;
	}
	if (imax_x > buffer.width)
	{
		imax_x = buffer.width;
	}
	if (imax_y > buffer.height)
	{
		imax_y = buffer.height;
	}

	u8* row = static_cast<u8*>(buffer.memory) + imin_x * buffer.bytes_per_pixel + imin_y * buffer.pitch;
	
	for (int y = imin_y; y < imax_y; ++y)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = imin_x; x < imax_x; ++x)
		{
			*pixel++ = color;
		}
		row += buffer.pitch;
	}
}

void PlatformLaunch()
{
	//Log::Init();
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		memory->is_initialized = true;
	}

	for (u64 controller_index = 0; controller_index < ArrayCount(input->controllers); ++controller_index)
	{
		const GameControllerInput& controller = input->controllers[controller_index];
		{
			if (controller.is_analog)
			{
			}
			else
			{}
		}
	}

	DrawRectangle(buffer, 0.f, 0.f, static_cast<r32>(buffer.width), static_cast<r32>(buffer.height), 0x00000000);
	DrawRectangle(buffer, 10.f, 10.f, 30.f, 30.f, 0xFFFFFFFF);

	GameOutputSound(sound_buffer, 400);
	//App::Run();
}