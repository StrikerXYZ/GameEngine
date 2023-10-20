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
		b32 is_analog;

		f32 start_x;
		f32 start_y;

		f32 min_x;
		f32 min_y;

		f32 max_x;
		f32 max_y;

		f32 end_x;
		f32 end_y;

		GameButtonState up;
		GameButtonState down;
		GameButtonState left;
		GameButtonState right;
		GameButtonState left_shoulder;
		GameButtonState right_shoulder;
	};

	struct GameInput
	{
		GameControllerInput controllers[4];
	};

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

