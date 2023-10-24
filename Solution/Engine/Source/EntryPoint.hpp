#pragma once

#include "Definition.hpp"

namespace Engine {

	struct GameOffscreenBuffer
	{
		void* memory;
		i32 width;
		i32 height;
		i32 pitch;
	};

	struct GameSoundBuffer
	{
		u32 samples_per_second;
		u32 sample_count;
		f32* samples;
	};

	struct GameButtonState
	{
		i32 num_half_transitions;
		b32 is_ended_down;
	};

	struct GameControllerInput
	{
		b32 is_connected;
		b32 is_analog;

		f32 stick_average_x;
		f32 stick_average_y;

		GameButtonState move_up;
		GameButtonState move_down;
		GameButtonState move_left;
		GameButtonState move_right;

		GameButtonState action_up;
		GameButtonState action_down;
		GameButtonState action_left;
		GameButtonState action_right;

		GameButtonState shoulder_left;
		GameButtonState shoulder_right;

		GameButtonState start;
		GameButtonState back;
	};

	struct GameInput
	{
		GameControllerInput controllers[5];
	};
	inline GameControllerInput& get_controller(GameInput& input, u64 controller_index)
	{
		Assert(controller_index < ArrayCount(input.controllers))
		return input.controllers[controller_index];
	}

	struct GameState
	{
		i32 x_offset;
		i32 y_offset;
		u32 tone_frequency;
	};

	struct GameMemory
	{
		b32 is_initialized;
		u64 permanent_storage_size;
		void* permanent_storage; //Note: required to be zeroed
		u64 transient_storage_size;
		void* transient_storage; //Note: required to be zeroed
	};

	void PlatformLaunch();
	void PlatformLoop(Engine::GameMemory* memory, const Engine::GameInput* input, const Engine::GameOffscreenBuffer& buffer, const Engine::GameSoundBuffer& sound_buffer);

	struct FileResult
	{
		u32 content_size;
		void* content;
	};

	FileResult PlatformRead(char* file_name);
	b32 PlatformWrite(char* file_name, Engine::FileResult& file);
	void PlatformFree(Engine::FileResult& file);
}

namespace App {
	extern void Run();
}

