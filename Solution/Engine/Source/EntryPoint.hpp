#pragma once

#include "Definition.hpp"

//namespace Engine {

	struct GameOffscreenBuffer
	{
		void* memory;
		i32 width;
		i32 height;
		i32 pitch;
		i32 bytes_per_pixel;
	};

	struct GameSoundBuffer
	{
		u32 samples_per_second;
		u32 sample_count;
		r32* samples;
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

		r32 stick_average_x;
		r32 stick_average_y;

		union
		{
			GameButtonState buttons[12];

			struct
			{
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
		};
	};

	struct GameInput
	{
		GameButtonState mouse_buttons[5];
		i32 mouse_x;
		i32 mouse_y;
		i32 mouse_z;

		r32 frame_delta;

		GameControllerInput controllers[5];
	};

	inline GameControllerInput& get_controller(GameInput& input, u64 controller_index)
	{
		Assert(controller_index < ArrayCount(input.controllers))
		return input.controllers[controller_index];
	}

	struct TileChunkPosition
	{
		u32 chunk_x;
		u32 chunk_y;

		u32 chunk_tile_x;
		u32 chunk_tile_y;
	};

	struct WorldPosition
	{
		u32 tile_x;
		u32 tile_y;
		r32 relative_x;
		r32 relative_y;
	};

	struct GameState
	{
		WorldPosition player_position;
	};

	struct TileChunk
	{
		u32* tiles;
	};

	struct World
	{
		u32 chunk_shift;
		u32 chunk_mask;
		i32 chunk_dimension;

		r32 tile_side_in_meters;
		r32 tile_radius_in_meters;
		i32 tile_side_in_pixels;
		r32 meters_to_pixels;

		u32 count_x;
		u32 count_y;
		TileChunk* tile_chunks;
	};

	struct FileResult
	{
		u32 content_size;
		void* content;
	};

	struct ThreadContext
	{
		int Placeholder;
	};

	//debug only
	#define PLATFORM_READ_FILE(name) FileResult name(ThreadContext& thread, const char* file_name)
	typedef PLATFORM_READ_FILE(FuncPlatformRead);

	//debug only
	#define PLATFORM_WRITE_FILE(name) b32 name(ThreadContext& thread, char* file_name, FileResult& file)
	typedef PLATFORM_WRITE_FILE(FuncPlatformWrite);

	//debug only
	#define PLATFORM_FREE_FILE(name) void name(ThreadContext& thread, FileResult& file)
	typedef PLATFORM_FREE_FILE(FuncPlatformFree);

	struct GameMemory
	{
		b32 is_initialized;
		u64 permanent_storage_size;
		void* permanent_storage; //Note: required to be zeroed
		u64 transient_storage_size;
		void* transient_storage; //Note: required to be zeroed

		FuncPlatformRead* Debug_PlatformRead;
		FuncPlatformWrite* Debug_PlatformWrite;
		FuncPlatformFree* Debug_PlatformFree;
	};

	#define GAME_LOOP(name) void name(ThreadContext& thread, GameMemory* memory, const GameInput* input, const GameOffscreenBuffer& buffer, const GameSoundBuffer& sound_buffer)
	typedef GAME_LOOP(FuncPlatformLoop);

	void PlatformLaunch();
//}

namespace App {
	//extern void Run() {};
}

