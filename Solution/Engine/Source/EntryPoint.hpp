#pragma once

#include "Definition.hpp"
#include "Tile.hpp"

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

	struct MemoryArena
	{
		MemoryIndex size;
		u8* base;
		MemoryIndex used;
	};

	internal_static void InitializeArena(MemoryArena& arena, MemoryIndex size, u8* base)
	{
		arena.size = size;
		arena.base = base;
		arena.used = 0;
	}

	internal_static void* PushSize_(MemoryArena& arena, MemoryIndex size)
	{
		Assert(arena.used + size <= arena.size);
		void* result = arena.base + arena.used;
		arena.used += size;
		return result;
	}
#define PushStruct(Arena, Type) static_cast<Type*>(PushSize_(Arena, sizeof(Type)))
#define PushArray(Arena, Count, Type) static_cast<Type*>(PushSize_(Arena, Count*sizeof(Type)))

	struct World
	{
		TileMap* tile_map;
	};

	struct GameState
	{
		MemoryArena world_arena;
		World* world;
		TileMapPosition player_position;
		u32* pixel_ptr;
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

	#define GAME_LOOP(name) void name(ThreadContext& thread, GameMemory* memory, const GameInput* input, const GameOffscreenBuffer& buffer)
	typedef GAME_LOOP(FuncPlatformLoop);

	#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext& thread, GameMemory* memory, const GameSoundBuffer& sound_buffer)
	typedef GAME_GET_SOUND_SAMPLES(FuncPlatformGetSoundSamples);

	void PlatformLaunch();
//}

namespace App {
	//extern void Run() {};
}

