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

internal_static TileMap* GetTileMap(World& world, i32 tile_map_x, i32 tile_map_y)
{
	TileMap* tile_map = nullptr;

	if (tile_map_x >= 0 && tile_map_x < world.count_x &&
		tile_map_y >= 0 && tile_map_y < world.count_y)
	{
		tile_map = &world.tile_maps[tile_map_y * world.count_x + tile_map_x];
	}

	return tile_map;
}

internal_static u32 GetTileValueUnchecked(World& world, TileMap& tile_map, i32 tile_x, i32 tile_y)
{
	Assert((tile_x >= 0 && tile_x < world.tile_map_columns &&
		tile_y >= 0 && tile_y < world.tile_map_rows));
	return tile_map.tiles[tile_y * world.tile_map_columns + tile_x];
}

internal_static b32 IsTileMapPointEmpty(World& world, TileMap& tile_map, i32 tile_x, i32 tile_y)
{
	b32 is_empty = false;

	if (tile_x >= 0 && tile_x < world.tile_map_columns &&
		tile_y >= 0 && tile_y < world.tile_map_rows)
	{
		is_empty = (GetTileValueUnchecked(world, tile_map, tile_x, tile_y) == 0);
	}

	return is_empty;
}

inline void RecanonicalizeCoord(World& world, i32 tile_count, i32& tile_map, i32& tile, r32& relative)
{
	r32 tile_side_in_meters = static_cast<r32>(world.tile_side_in_meters);

	i32 offset = FloorToI32(relative / tile_side_in_meters);
	tile += offset;
	relative -= static_cast<r32>(offset) * tile_side_in_meters;

	Assert(relative >= 0);
	Assert(relative < tile_side_in_meters);

	if (tile < 0)
	{
		tile += tile_count;
		--tile_map;
	}
	if (tile >= tile_count)
	{
		tile -= tile_count;
		++tile_map;
	}
}

inline WorldPosition RecanonicalizePosition(World& world, WorldPosition position)
{
	WorldPosition canonical_position = position;

	RecanonicalizeCoord(world, world.tile_map_columns, canonical_position.tile_map_x, canonical_position.tile_x, canonical_position.relative_x);
	RecanonicalizeCoord(world, world.tile_map_rows, canonical_position.tile_map_y, canonical_position.tile_y, canonical_position.relative_y);

	return canonical_position;
}

internal_static b32 IsWorldPointEmpty(World& world, WorldPosition position)
{
	b32 is_empty = false;

	TileMap* tile_map = GetTileMap(world, position.tile_map_x, position.tile_map_y);
	Assert(tile_map);
	return IsTileMapPointEmpty(world, *tile_map, position.tile_x, position.tile_y);
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	const i32 tile_map_columns = 17;
	const i32 tile_map_rows = 9;

	u32 tiles00[tile_map_rows][tile_map_columns]
	{
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 1, 1 },
		{ 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0, 0 },
		{ 1, 0, 0, 0,  0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 }
	};

	u32 tiles01[tile_map_rows][tile_map_columns]
	{
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1 }
	};

	u32 tiles10[tile_map_rows][tile_map_columns]
	{
		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 }
	};

	u32 tiles11[tile_map_rows][tile_map_columns]
	{
		{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1 ,1 ,1 ,1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1 },

		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0, 1 },
		{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1 }
	};

	TileMap tile_maps[2][2];
	tile_maps[0][0].tiles = reinterpret_cast<u32*>(tiles00);
	tile_maps[0][1].tiles = reinterpret_cast<u32*>(tiles01);
	tile_maps[1][0].tiles = reinterpret_cast<u32*>(tiles10);
	tile_maps[1][1].tiles = reinterpret_cast<u32*>(tiles11);

	World world;
	world.count_y = 2;
	world.count_x = 2;
	world.tile_map_columns = tile_map_columns;
	world.tile_map_rows = tile_map_rows;

	world.tile_side_in_meters = 1.4f;
	world.tile_side_in_pixels = 60;
	world.meters_to_pixels = static_cast<r32>(world.tile_side_in_pixels) / world.tile_side_in_meters;

	r32 tile_side_in_pixels = static_cast<r32>(world.tile_side_in_pixels);
	world.lower_left_x = { -tile_side_in_pixels / 2 };
	world.lower_left_y = static_cast<r32>(buffer.height);

	r32 player_height = 1.4f;
	r32 player_width = player_height * 0.75f;

	world.tile_maps = reinterpret_cast<TileMap*>(&tile_maps);

	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		game_state->player_position.tile_map_x = 0;
		game_state->player_position.tile_map_y = 0;
		game_state->player_position.tile_x = 2;
		game_state->player_position.tile_y = 2;
		game_state->player_position.relative_x = .5f;
		game_state->player_position.relative_y = .5f;
		memory->is_initialized = true;
	}

	TileMap* tile_map = GetTileMap(world, game_state->player_position.tile_map_x, game_state->player_position.tile_map_y);
	Assert(tile_map);

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
				delta_player_x *= 2;
				delta_player_y *= 2;

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

	for (int row = 0; row < tile_map_rows; ++row)
	{
		for (int column = 0; column < tile_map_columns; ++column)
		{
			u32 tile_id = GetTileValueUnchecked(world, *tile_map, column, row);
			r32 shade = (tile_id == 1)? 1.f : 0.5f;

			if (column == game_state->player_position.tile_x && row == game_state->player_position.tile_y)
				shade = 0.0f;

			r32 min_x = world.lower_left_x + static_cast<r32>(column * world.tile_side_in_pixels);
			r32 min_y = world.lower_left_y - static_cast<r32>(row * world.tile_side_in_pixels);
			r32 max_x = min_x + static_cast<r32>(world.tile_side_in_pixels);
			r32 max_y = min_y - static_cast<r32>(world.tile_side_in_pixels);

			DrawRectangle(buffer, min_x, max_y, max_x, min_y, shade, shade, shade);
		}
	}

	r32 player_left = world.lower_left_x + static_cast<r32>(game_state->player_position.tile_x * world.tile_side_in_pixels) +
		world.meters_to_pixels * (game_state->player_position.relative_x - player_width * .5f);
	r32 player_top = world.lower_left_y - static_cast<r32>(game_state->player_position.tile_y * world.tile_side_in_pixels) -
		world.meters_to_pixels * (game_state->player_position.relative_y + player_height);
	DrawRectangle(buffer, player_left, 
		player_top, 
		player_left + player_width * world.meters_to_pixels,
		player_top + player_height * world.meters_to_pixels,
		1.f, 0.f, 0.f);


	GameOutputSound(sound_buffer, 400);
	//App::Run();
}