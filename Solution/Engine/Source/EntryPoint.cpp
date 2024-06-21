#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "Intrinsics.hpp"

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

internal_static void DrawRectangle(const GameOffscreenBuffer& buffer, 
	r32 min_x, r32 min_y, r32 max_x, r32 max_y,
	r32 colour_r, r32 colour_g, r32 colour_b
)
{
	i32 imin_x = RoundToI32(min_x);
	i32 imin_y = RoundToI32(min_y);
	i32 imax_x = RoundToI32(max_x);
	i32 imax_y = RoundToI32(max_y);

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

	u32 colour = RoundToU32(colour_r * 255.f) << 16 
		| RoundToU32(colour_g * 255.f) << 8
		| RoundToU32(colour_b * 255.f);

	u8* row = static_cast<u8*>(buffer.memory) + imin_x * buffer.bytes_per_pixel + imin_y * buffer.pitch;
	
	for (int y = imin_y; y < imax_y; ++y)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = imin_x; x < imax_x; ++x)
		{
			*pixel++ = colour;
		}
		row += buffer.pitch;
	}
}

void PlatformLaunch()
{
	//Log::Init();
}

internal_static TileChunk* GetTileChunk(World& world, u32 tile_chunk_x, u32 tile_chunk_y)
{
	TileChunk* tile_chunk = nullptr;

	if (tile_chunk_x < world.count_x &&
		tile_chunk_y < world.count_y)
	{
		tile_chunk = &world.tile_chunks[tile_chunk_y * world.count_x + tile_chunk_x];
	}

	return tile_chunk;
}

internal_static u32 GetTileValueUnchecked(World& world, TileChunk& tile_chunk, u32 tile_x, u32 tile_y)
{
	Assert(tile_x < static_cast<u32>(world.chunk_dimension))
	Assert(tile_y < static_cast<u32>(world.chunk_dimension));
	return tile_chunk.tiles[static_cast<i32>(tile_y) * world.chunk_dimension + static_cast<i32>(tile_x)];
}

internal_static b32 GetTileValue(World& world, TileChunk& tile_chunk, u32 tile_x, u32 tile_y)
{
	u32 chunk_value = 0;

	chunk_value = GetTileValueUnchecked(world, tile_chunk, tile_x, tile_y);

	return chunk_value;
}

inline void RecanonicalizeCoord(World& world, u32& tile, r32& relative)
{
	r32 tile_side_in_meters = static_cast<r32>(world.tile_side_in_meters);

	//Note: World is assumed to be toroidal topology
	i32 offset = RoundToI32(relative / tile_side_in_meters);
	tile = static_cast<u32>(static_cast<i32>(tile) + offset);
	relative -= static_cast<r32>(offset) * tile_side_in_meters;

	Assert(relative >= tile_side_in_meters * -.5f);
	Assert(relative <= tile_side_in_meters * .5f);
}

inline WorldPosition RecanonicalizePosition(World& world, WorldPosition position)
{
	WorldPosition canonical_position = position;

	RecanonicalizeCoord(world, canonical_position.tile_x, canonical_position.relative_x);
	RecanonicalizeCoord(world, canonical_position.tile_y, canonical_position.relative_y);

	return canonical_position;
}

internal_static TileChunkPosition GetChunkPosition(World& world, u32 x, u32 y)
{
	TileChunkPosition result;
	result.chunk_x = x >> world.chunk_shift;
	result.chunk_y = y >> world.chunk_shift;
	result.chunk_tile_x = x & world.chunk_mask;
	result.chunk_tile_y = y & world.chunk_mask;
	return result;
}

internal_static u32 GetTileValue(World& world, u32 tile_x, u32 tile_y)
{
	b32 is_empty = false;

	TileChunkPosition chunk_position = GetChunkPosition(world, tile_x, tile_y);
	TileChunk* tile_chunk = GetTileChunk(world, chunk_position.chunk_x, chunk_position.chunk_y);
	return (tile_chunk)? GetTileValue(world, *tile_chunk, chunk_position.chunk_tile_x, chunk_position.chunk_tile_y) : 0;
}

internal_static b32 IsWorldPointEmpty(World& world, WorldPosition position)
{
	u32 tile_value = GetTileValue(world, position.tile_x, position.tile_y);
	b32 is_empty = (tile_value == 0);
	return is_empty;
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	const i32 tile_map_columns = 256;
	const i32 tile_map_rows = 256;

	u32 tiles[tile_map_rows][tile_map_columns]
	{
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1 ,1 ,1 ,1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 1, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 },

		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1 ,1 ,1 ,1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 }
	};

	World world;
	//Note: using 256x256 tile chunks
	world.chunk_shift = 8;
	world.chunk_mask = (1 << world.chunk_shift) - 1; //create mask be reverse shift and minus 1
	world.chunk_dimension = 256;

	world.count_y = 1;
	world.count_x = 1;

	TileChunk chunk;
	chunk.tiles = reinterpret_cast<u32*>(tiles);
	world.tile_chunks = &chunk;

	world.tile_side_in_meters = 1.4f;
	world.tile_side_in_pixels = 60;
	world.meters_to_pixels = static_cast<r32>(world.tile_side_in_pixels) / world.tile_side_in_meters;

	r32 tile_side_in_pixels = static_cast<r32>(world.tile_side_in_pixels);
	r32 lower_left_x = { -tile_side_in_pixels / 2 };
	r32 lower_left_y = static_cast<r32>(buffer.height);

	r32 player_height = 1.4f;
	r32 player_width = player_height * 0.75f;

	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		game_state->player_position.tile_x = 2;
		game_state->player_position.tile_y = 2;
		game_state->player_position.relative_x = .5f;
		game_state->player_position.relative_y = .5f;
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
			{
				r32 delta_player_x{ 0.f };
				r32 delta_player_y{ 0.f };

				if (controller.move_left.is_ended_down)
				{
					delta_player_x = -1.f;
				}
				if (controller.move_up.is_ended_down)
				{
					delta_player_y = 1.f;
				}
				if (controller.move_right.is_ended_down)
				{
					delta_player_x = 1.f;
				}
				if (controller.move_down.is_ended_down)
				{
					delta_player_y = -1.f;
				}

				r32 player_speed = 3.f;

				if (controller.action_up.is_ended_down)
				{
					player_speed = 10.f;
				}

				delta_player_x *= player_speed;
				delta_player_y *= player_speed;

				auto new_position = game_state->player_position;
				new_position.relative_x += delta_player_x * input->frame_delta;
				new_position.relative_y += delta_player_y * input->frame_delta;
				new_position = RecanonicalizePosition(world, new_position);

				auto position_left = new_position;
				position_left.relative_x -= 0.5f * player_width;
				position_left = RecanonicalizePosition(world, position_left);

				auto position_right = new_position;
				position_right.relative_x += 0.5f * player_width;
				position_right = RecanonicalizePosition(world, position_right);

				if (IsWorldPointEmpty(world, new_position) &&
					IsWorldPointEmpty(world, position_left) &&
					IsWorldPointEmpty(world, position_right))
				{
					game_state->player_position = new_position;
				}

			}
		}
	}

	DrawRectangle(buffer, 0.f, 0.f, static_cast<r32>(buffer.width), static_cast<r32>(buffer.height), 1.f, 0.f, 1.f);

	r32 screen_center_x = .5f * static_cast<r32>(buffer.width);
	r32 screen_center_y = .5f * static_cast<r32>(buffer.height);

	for (i32 relative_row = -10; relative_row < 10; ++relative_row)
	{
		for (i32 relative_column = -20; relative_column < 20; ++relative_column)
		{
			i32 signed_column = relative_column + static_cast<i32>(game_state->player_position.tile_x);
			i32 signed_row = relative_row + static_cast<i32>(game_state->player_position.tile_y);

			u32 column = static_cast<u32>(signed_column);
			u32 row = static_cast<u32>(signed_row);

			u32 tile_id = GetTileValue(world, column, row);
			r32 shade = (tile_id == 1)? 1.f : 0.5f;

			if (column == game_state->player_position.tile_x &&
				row == game_state->player_position.tile_y)
			{
				shade = 0.0f;
			}

			r32 center_x = screen_center_x
				- game_state->player_position.relative_x * world.meters_to_pixels
				+ static_cast<r32>(relative_column) * tile_side_in_pixels;
			r32 center_y = screen_center_y
				+ game_state->player_position.relative_y * world.meters_to_pixels
				- static_cast<r32>(relative_row) * tile_side_in_pixels;
			r32 min_x = center_x - tile_side_in_pixels * .5f;
			r32 min_y = center_y - tile_side_in_pixels * .5f;
			r32 max_x = center_x + tile_side_in_pixels * .5f;
			r32 max_y = center_y + tile_side_in_pixels * .5f;

			DrawRectangle(buffer, min_x, min_y, max_x, max_y, shade, shade, shade);
		}
	}

	r32 player_left = screen_center_x -
		world.meters_to_pixels * (player_width * .5f);
	r32 player_top = screen_center_y  -
		world.meters_to_pixels * (player_height);
	DrawRectangle(buffer, player_left, 
		player_top, 
		player_left + player_width * world.meters_to_pixels,
		player_top + player_height * world.meters_to_pixels,
		1.f, 0.f, 0.f);


	GameOutputSound(sound_buffer, 400);
	//App::Run();
}