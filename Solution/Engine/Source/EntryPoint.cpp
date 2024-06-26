#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "Intrinsics.hpp"
#include "Tile.hpp"
#include "Random.hpp"

#include "Tile.cpp"

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

#pragma pack(push, 1)
struct BitmapHeader
{
	u16 file_type;
	u32 file_size;
	u16 reserved_1;
	u16 reserved_2;
	u32 bitmap_offset;

	u32 size;
	i32 width;
	i32 height;
	u16 plane;
	u16 bits_per_pixel;

	u32 compression;
	u32 size_of_bitmap;
	i32 horizontal_resolution;
	i32 vertical_resolution;
	u32 colours_used;
	u32 colours_important;

	u32 red_mask;
	u32 green_mask;
	u32 blue_mask;
};
#pragma pack(pop)

internal_static LoadedBitmap Debug_LoadBMP(ThreadContext& thread, FuncPlatformRead* read_entire_file, char* filename)
{
	LoadedBitmap result{};

	const FileResult read_result = read_entire_file(thread, filename);

	if (read_result.content_size != 0)
	{
		const auto* bitmap_header = static_cast<BitmapHeader*>(read_result.content);
		
		u32* pixels = reinterpret_cast<u32*>(static_cast<u8*>(read_result.content) + bitmap_header->bitmap_offset);
		result.pixels = pixels;
		result.width = bitmap_header->width;
		result.height = bitmap_header->height;

		Assert(bitmap_header->compression == 3);

		u32 red_mask = bitmap_header->red_mask;
		u32 green_mask = bitmap_header->green_mask;
		u32 blue_mask = bitmap_header->blue_mask;
		u32 alpha_mask = ~(red_mask | green_mask | blue_mask);

		BitScanResult red_shift = FindLeastSignificantSetBit(red_mask);
		BitScanResult green_shift = FindLeastSignificantSetBit(green_mask);
		BitScanResult blue_shift = FindLeastSignificantSetBit(blue_mask);
		BitScanResult alpha_shift = FindLeastSignificantSetBit(alpha_mask);

		Assert(red_shift.found);
		Assert(green_shift.found);
		Assert(blue_shift.found);
		Assert(alpha_shift.found);

		u32* source_dest = pixels;
		for (i32 y = 0; y < bitmap_header->height; ++y)
		{
			for (i32 x = 0; x < bitmap_header->width; ++x)
			{
				u32 c = *source_dest;
				*source_dest = (((c >> alpha_shift.index) & 0xFF) << 24) |
					(((c >> red_shift.index) & 0xFF) << 16) |
					(((c >> green_shift.index) & 0xFF) << 8) |
					(((c >> blue_shift.index) & 0xFF) << 0);
				++source_dest;
			}
		}

	}

	return result;

}

internal_static void LoadBMP(const GameOffscreenBuffer& buffer, 
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

void DrawBitmap(const GameOffscreenBuffer& buffer, LoadedBitmap& bitmap, r32 in_x, r32 in_y, i32 align_x = 0, i32 align_y = 0)
{
	in_x -= static_cast<float>(align_x);
	in_y -= static_cast<float>(align_y);

	i32 imin_x = RoundToI32(in_x);
	i32 imin_y = RoundToI32(in_y);
	i32 imax_x = RoundToI32(in_x + static_cast<r32>(bitmap.width));
	i32 imax_y = RoundToI32(in_y + static_cast<r32>(bitmap.height));

	i32 source_offset_x = 0;
	if (imin_x < 0)
	{
		source_offset_x = -imin_x;
		imin_x = 0;
	}

	i32 source_offset_y = 0;
	if (imin_y < 0)
	{
		source_offset_y = -imin_y;
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

	u32* source_row = bitmap.pixels + bitmap.width * (bitmap.height - 1);
	source_row += -source_offset_y * bitmap.width + source_offset_x;
	u8* dest_row = static_cast<u8*>(buffer.memory) + imin_x * buffer.bytes_per_pixel + imin_y * buffer.pitch;
	for (i32 y = imin_y; y < imax_y; ++y)
	{
		u32* dest = reinterpret_cast<u32*>(dest_row);
		u32* source = source_row;
		for (i32 x = imin_x; x < imax_x; ++x)
		{
			r32 alpha = static_cast<float>((*source >> 24) & 0xFF) / 255.f;

			r32 source_red = static_cast<float>((*source >> 16) & 0xFF);
			r32 source_green = static_cast<float>((*source >> 8) & 0xFF);
			r32 source_blue = static_cast<float>((*source >> 0) & 0xFF);

			r32 dest_red = static_cast<float>((*source >> 16) & 0xFF);
			r32 dest_green = static_cast<float>((*source >> 8) & 0xFF);
			r32 dest_blue = static_cast<float>((*source >> 0) & 0xFF);

			r32 r = (1.f - alpha) * source_red + alpha * dest_red;
			r32 g = (1.f - alpha) * source_green + alpha * dest_green;
			r32 b = (1.f - alpha) * source_blue + alpha * dest_blue;

			if ((*source >> 24) > 128)
			{
				*dest = (static_cast<u32>(r + 0.5f) << 16 | 
					static_cast<u32>(g + 0.5f) << 8 |
					static_cast<u32>(b + 0.5f) << 0);
			}

			++dest;
			++source;
		}

		dest_row += buffer.pitch;
		source_row -= bitmap.width;
	}
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	r32 player_height = 1.4f;
	r32 player_width = player_height * 0.75f;

	u32 tiles_per_width = 17;
	u32 tiles_per_height = 9;

	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		game_state->backdrop = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_background.bmp");

		HeroBitmap* bitmap;

		bitmap = game_state->hero_bitmaps;
		bitmap->head = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_head_right.bmp");
		bitmap->body = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_body.bmp");
		bitmap->align_x = 74;
		bitmap->align_y = 198;

		++bitmap;
		bitmap->head = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_head_back.bmp");
		bitmap->body = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_body.bmp");
		bitmap->align_x = 74;
		bitmap->align_y = 198;

		++bitmap;
		bitmap->head = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_head_left.bmp");
		bitmap->body = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_body.bmp");
		bitmap->align_x = 74;
		bitmap->align_y = 198;

		++bitmap;
		bitmap->head = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_head.bmp");
		bitmap->body = Debug_LoadBMP(thread, memory->Debug_PlatformRead, "test/test_body.bmp");
		bitmap->align_x = 74;
		bitmap->align_y = 198;

		game_state->camera_position.tile_x = 17/2;
		game_state->camera_position.tile_y = 9/2;
		//game_state->camera_position.offset_x = .5f;
		//game_state->camera_position.offset_y = .5f;

		game_state->player_position.tile_x = 2;
		game_state->player_position.tile_y = 2;
		game_state->player_position.offset_x = .5f;
		game_state->player_position.offset_y = .5f;

		InitializeArena(game_state->world_arena, 
			memory->permanent_storage_size - sizeof(GameState),
			static_cast<u8*>(memory->permanent_storage) + sizeof(GameState));

		game_state->world = PushStruct(game_state->world_arena, World);
		World* world = game_state->world;
		world->tile_map = PushStruct(game_state->world_arena, TileMap);

		TileMap& tile_map = *(world->tile_map);
		//Note: using 16x16 tile chunks
		tile_map.chunk_shift = 4;
		tile_map.chunk_mask = (1 << tile_map.chunk_shift) - 1; //create mask be reverse shift and minus 1
		tile_map.chunk_dimension = (1 << tile_map.chunk_shift);

		tile_map.count_y = 128;
		tile_map.count_x = 128;
		tile_map.count_z = 2;
		tile_map.tile_chunks = PushArray(game_state->world_arena, 
			tile_map.count_x * tile_map.count_y * tile_map.count_z, 
			TileChunk);

		tile_map.tile_side_in_meters = 1.4f;

		u32 screen_x = 0;
		u32 screen_y = 0;

		u32 random_number_index = 0;

		b32 door_left = false;
		b32 door_top = false;
		b32 door_right = false;
		b32 door_bottom = false;
		b32 door_up = false;
		b32 door_down = false;

		u32 abs_tile_z = 0;

		for (u32 screen_index = 0; screen_index < 100; ++screen_index)
		{
			Assert(random_number_index < ArrayCount(random_number_table));
			u32 random_choice;
			if (door_up || door_down)
			{
				random_choice = random_number_table[random_number_index++] % 2;
			}
			else
			{
				random_choice = random_number_table[random_number_index++] % 3;
			}

			b32 created_z_door = false;
			if (random_choice == 2)
			{
				created_z_door = true;
				if (abs_tile_z == 0)
				{
					door_up = true;
				}
				else
				{
					door_down = true;
				}
			}
			else if (random_choice == 1)
			{
				door_right = true;
			}
			else
			{
				door_top = true;
			}

			for (u32 tile_y = 0; tile_y < tiles_per_height; ++tile_y)
			{
				for (u32 tile_x = 0; tile_x < tiles_per_width; ++tile_x)
				{
					u32 abs_tile_x = screen_x * tiles_per_width + tile_x;
					u32 abs_tile_y = screen_y * tiles_per_height + tile_y;

					u32 tile_value = 1;
					if (tile_x == 0)
					{
						if (!door_left || tile_y != tiles_per_height / 2)
						{
							tile_value = 2;
						}
					}

					if(tile_x == (tiles_per_width - 1))
					{
						if (!door_right || tile_y != tiles_per_height / 2)
						{
							tile_value = 2;
						}
					}
					if (tile_y == 0)
					{
						if (!door_bottom || tile_x != tiles_per_width / 2)
						{
							tile_value = 2;
						}
					}

					if(tile_y == (tiles_per_height - 1))
					{
						if (!door_top || tile_x != tiles_per_width / 2)
						{
							tile_value = 2;
						}
					}

					if (tile_x == 10 && tile_y == 6)
					{
						if (door_up)
						{
							tile_value = 3;
						}
						if (door_down)
						{
							tile_value = 4;
						}
					}

					SetTileValue(game_state->world_arena, *(world->tile_map), abs_tile_x, abs_tile_y, abs_tile_z, tile_value);
				}
			}

			door_left = door_right;
			door_bottom = door_top;
			
			if (created_z_door)
			{
				door_up = !door_up;
				door_down = !door_down;
			}
			else
			{
				door_up = false;
				door_down = false;
			}

			door_top = false;
			door_right = false;

			if (random_choice == 2)
			{
				if (abs_tile_z == 0)
				{
					abs_tile_z = 1;
				}
				else
				{
					abs_tile_z = 0;
				}
			}
			else if (random_choice == 1)
			{
				screen_x += 1;
			}
			else
			{
				screen_y += 1;
			}
		}

		memory->is_initialized = true;
	}

	World& world = *(game_state->world);
	TileMap& tile_map = *(world.tile_map);

	i32 tile_side_in_pixels = 60;
	r32 r_tile_side_in_pixels = static_cast<r32>(tile_side_in_pixels);
	r32 meters_to_pixels = static_cast<r32>(tile_side_in_pixels) / tile_map.tile_side_in_meters;

	r32 lower_left_x = -r_tile_side_in_pixels/2;
	r32 lower_left_y = static_cast<r32>(buffer.height);

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
					game_state->facing_direction = 2;
					delta_player_x = -1.f;
				}
				if (controller.move_up.is_ended_down)
				{
					game_state->facing_direction = 1;
					delta_player_y = 1.f;
				}
				if (controller.move_right.is_ended_down)
				{
					game_state->facing_direction = 0;
					delta_player_x = 1.f;
				}
				if (controller.move_down.is_ended_down)
				{
					game_state->facing_direction = 3;
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
				new_position.offset_x += delta_player_x * input->frame_delta;
				new_position.offset_y += delta_player_y * input->frame_delta;
				new_position = RecanonicalizePosition(tile_map, new_position);

				auto position_left = new_position;
				position_left.offset_x -= 0.5f * player_width;
				position_left = RecanonicalizePosition(tile_map, position_left);

				auto position_right = new_position;
				position_right.offset_x += 0.5f * player_width;
				position_right = RecanonicalizePosition(tile_map, position_right);

				if (IsTileMapPointEmpty(tile_map, new_position) &&
					IsTileMapPointEmpty(tile_map, position_left) &&
					IsTileMapPointEmpty(tile_map, position_right))
				{
					if (!IsSameTile(game_state->player_position, new_position))
					{
						u32 tile_value = GetTileValue(tile_map, new_position);
						if (tile_value == 3)
						{
							++new_position.tile_z;
						}
						else if (tile_value == 4)
						{
							--new_position.tile_z;
						}
					}

					game_state->player_position = new_position;
				}

				game_state->camera_position.tile_z = game_state->player_position.tile_z;

				TileMapDifference difference = Subtract(tile_map, game_state->player_position, game_state->camera_position);
				if (difference.delta_x > 9.f * tile_map.tile_side_in_meters)
				{
					game_state->camera_position.tile_x += tiles_per_width;
				}
				else if (difference.delta_x < -9.f * tile_map.tile_side_in_meters)
				{
					game_state->camera_position.tile_x -= tiles_per_width;
				}
				if (difference.delta_y > 5.f * tile_map.tile_side_in_meters)
				{
					game_state->camera_position.tile_y += tiles_per_height;
				}
				else if (difference.delta_y < -5.f * tile_map.tile_side_in_meters)
				{
					game_state->camera_position.tile_y -= tiles_per_height;
				}
				//game_state->camera_position.tile_y = game_state->player_position.tile_z;
			}
		}
	}

	DrawBitmap(buffer, game_state->backdrop, 0.f, 0.f);

	r32 screen_center_x = .5f * static_cast<r32>(buffer.width);
	r32 screen_center_y = .5f * static_cast<r32>(buffer.height);

	for (i32 relative_row = -10; relative_row < 10; ++relative_row)
	{
		for (i32 relative_column = -20; relative_column < 20; ++relative_column)
		{
			i32 signed_column = relative_column + static_cast<i32>(game_state->camera_position.tile_x);
			i32 signed_row = relative_row + static_cast<i32>(game_state->camera_position.tile_y);

			u32 column = static_cast<u32>(signed_column);
			u32 row = static_cast<u32>(signed_row);

			u32 tile_id = GetTileValue(tile_map, column, row, game_state->camera_position.tile_z);

			if (tile_id > 1)
			{
				r32 shade = (tile_id == 2) ? 1.f : 0.5f;

				if (tile_id > 2)
				{
					shade = 0.25f;
				}

				if (column == game_state->camera_position.tile_x &&
					row == game_state->camera_position.tile_y)
				{
					shade = 0.0f;
				}

				r32 center_x = screen_center_x
					- game_state->camera_position.offset_x * meters_to_pixels
					+ static_cast<r32>(relative_column) * r_tile_side_in_pixels;
				r32 center_y = screen_center_y
					+ game_state->camera_position.offset_y * meters_to_pixels
					- static_cast<r32>(relative_row) * r_tile_side_in_pixels;
				r32 min_x = center_x - r_tile_side_in_pixels * .5f;
				r32 min_y = center_y - r_tile_side_in_pixels * .5f;
				r32 max_x = center_x + r_tile_side_in_pixels * .5f;
				r32 max_y = center_y + r_tile_side_in_pixels * .5f;

				DrawRectangle(buffer, min_x, min_y, max_x, max_y, shade, shade, shade);
			}
		}
	}

	TileMapDifference difference = Subtract(tile_map, game_state->player_position, game_state->camera_position);

	r32 player_gound_point_x = screen_center_x + meters_to_pixels * difference.delta_x;
	r32 player_gound_point_y = screen_center_y - meters_to_pixels * difference.delta_y;
	r32 player_left = player_gound_point_x - meters_to_pixels * (player_width * .5f);
	r32 player_top = player_gound_point_y - meters_to_pixels * (player_height);
	DrawRectangle(buffer, player_left, 
		player_top, 
		player_left + player_width * meters_to_pixels,
		player_top + player_height * meters_to_pixels,
		1.f, 0.f, 0.f);

	HeroBitmap& hero_bitmap = game_state->hero_bitmaps[game_state->facing_direction];
	DrawBitmap(buffer, hero_bitmap.body, player_gound_point_x, player_gound_point_y, hero_bitmap.align_x, hero_bitmap.align_y);
	DrawBitmap(buffer, hero_bitmap.head, player_gound_point_x, player_gound_point_y, hero_bitmap.align_x, hero_bitmap.align_y);

	//App::Run();
}

extern "C"
ENGINE_API GAME_GET_SOUND_SAMPLES(PlatformGetSoundSamples)
{
	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	GameOutputSound(sound_buffer, 400);
}