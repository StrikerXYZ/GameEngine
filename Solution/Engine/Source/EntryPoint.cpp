#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"


internal_static void GameOutputSound(const GameSoundBuffer& buffer, u32 tone_frequency)
{
	local_static f32 t_sine;
	i16 tone_volume = 1;
	f32 wave_period = static_cast<f32>(buffer.samples_per_second / tone_frequency);
	f32 pi = 3.14159265f;
	
	f32* sample_out = buffer.samples;
	for (u32 i = 0; i < buffer.sample_count; ++i)
	{
		f32 sine_value = 0;//sinf(t_sine);
		f32 sample_value = sine_value * tone_volume;
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
		FileResult bitmap_memory = memory->Debug_PlatformRead(thread, __FILE__);
		if (bitmap_memory.content)
		{
			memory->Debug_PlatformFree(thread, bitmap_memory);
		}

		i32 tone_frequency = 256;
		memory->is_initialized = true;
	}

	for (u64 controller_index = 0; controller_index < ArrayCount(input->controllers); ++controller_index)
	{
		const GameControllerInput& controller = input->controllers[controller_index];
		{
			if (controller.is_analog)
			{
				game_state->tone_frequency = 256 + static_cast<u32>(128 * controller.stick_average_x);
				game_state->x_offset += static_cast<i32>(4.f * controller.stick_average_y);
			}
			else
			{
				game_state->tone_frequency = 256;
				game_state->x_offset += static_cast<i32>(controller.move_right.is_ended_down) - static_cast<i32>(controller.move_left.is_ended_down);
			}

			if (controller.action_down.is_ended_down)
			{
				game_state->y_offset += 1;
			}
		}
	}

	GameOutputSound(sound_buffer, game_state->tone_frequency);
	RenderGradient(buffer, game_state->x_offset, game_state->y_offset);

	//App::Run();
}