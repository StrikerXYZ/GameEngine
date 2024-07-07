#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "Intrinsics.hpp"
#include "Tile.hpp"
#include "Random.hpp"
#include "Math.hpp"

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

internal_static void DrawRectangle(const GameOffscreenBuffer& buffer, V2 min, V2 max, r32 colour_r, r32 colour_g, r32 colour_b
)
{
	i32 imin_x = RoundToI32(min.x);
	i32 imin_y = RoundToI32(min.y);
	i32 imax_x = RoundToI32(max.x);
	i32 imax_y = RoundToI32(max.y);

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

		BitScanResult red_scan = FindLeastSignificantSetBit(red_mask);
		BitScanResult green_scan = FindLeastSignificantSetBit(green_mask);
		BitScanResult blue_scan = FindLeastSignificantSetBit(blue_mask);
		BitScanResult alpha_scan = FindLeastSignificantSetBit(alpha_mask);

		Assert(red_scan.found);
		Assert(green_scan.found);
		Assert(blue_scan.found);
		Assert(alpha_scan.found);

		i32 red_shift = 16 - static_cast<i32>(red_scan.index);
		i32 green_shift = 8 - static_cast<i32>(green_scan.index);
		i32 blue_shift = 0 - static_cast<i32>(blue_scan.index);
		i32 alpha_shift = 24 - static_cast<i32>(alpha_scan.index);

		u32* source_dest = pixels;
		for (i32 y = 0; y < bitmap_header->height; ++y)
		{
			for (i32 x = 0; x < bitmap_header->width; ++x)
			{
				u32 c = *source_dest;

#if 0
				* source_dest++ = (((c >> alpha_scan.index) & 0xFF) << 24) |
					(((c >> red_scan.index) & 0xFF) << 16) |
					(((c >> green_scan.index) & 0xFF) << 8) |
					(((c >> blue_scan.index) & 0xFF) << 0);
#else
				* source_dest++ = (
					RotateLeft(c & red_mask, red_shift) |
					RotateLeft(c & green_mask, green_shift) |
					RotateLeft(c & blue_mask, blue_shift) |
					RotateLeft(c & alpha_mask, alpha_shift)
				);
#endif
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

internal_static u32 AddEntity(GameState& game_state)
{
	u32 entity_index = game_state.entity_count++;
	Assert(game_state.entity_count < ArrayCount(game_state.entities));
	Entity& entity = game_state.entities[entity_index];
	entity = {};
	return entity_index;
}

internal_static Entity* GetEntity(GameState& game_state, u32 index)
{
	Entity* entity = nullptr;

	if (index > 0 && index < ArrayCount(game_state.entities))
	{
		entity = &game_state.entities[index];
	}

	return entity;
}

internal_static void InitializePlayer(GameState& game_state, u32 entity_index)
{
	Entity* entity = GetEntity(game_state, entity_index);
	entity->exists = true;
	entity->position.tile_x = 2;
	entity->position.tile_y = 2;
	entity->position.offset_ = { .5f , .5f };

	entity->height = 1.4f;
	entity->width = entity->height * 0.75f;

	if (!GetEntity(game_state, game_state.camera_follow_entity_index))
	{
		game_state.camera_follow_entity_index = entity_index;
	}
}

internal_static void TestWall(r32 wall_x, r32 relative_x, r32 relative_y, r32 player_delta_x, r32 player_delta_y, r32& min_time, r32 min_y, r32 max_y)
{
	r32 time_epsilon = 0.0001f;
	if (player_delta_x != 0.f)
	{
		r32 result_time = (wall_x - relative_x) / player_delta_x;
		r32 y = relative_y * result_time * player_delta_y;
		if (result_time >= 0.f && result_time < min_time)
		{
			if (y >= min_y && y <= max_y)
			{
				min_time = Maximum(0.f, result_time - time_epsilon);
			}
		}
	}
}

internal_static void MovePlayer(GameState& game_state, Entity& entity, r32 delta_time, V2 acceleration)
{
	TileMap& tile_map = *game_state.world->tile_map;

	auto acceleration_length_squared = LengthSquared(acceleration);
	if (acceleration_length_squared > 1.0f)
	{
		//get unit vector
		acceleration *= OneOverSquareRoot(acceleration_length_squared);
	}

	r32 player_speed = 50.f; //m/s^2

	acceleration *= player_speed;

	acceleration -= 8.0f * entity.velocity;

	auto old_position = entity.position;

	//p' = 1/2 at^2 + vt + p
	V2 player_delta = 0.5f * acceleration * Square(delta_time) + entity.velocity * delta_time;

	//v' = at + v
	entity.velocity = acceleration * delta_time + entity.velocity;

	TileMapPosition new_position = Offset(tile_map, old_position, player_delta);

#if 0

	auto position_left = new_position;
	position_left.offset.x -= 0.5f * entity.width;
	position_left = RecanonicalizePosition(tile_map, position_left);

	auto position_right = new_position;
	position_right.offset.x += 0.5f * entity.width;
	position_right = RecanonicalizePosition(tile_map, position_right);

	b32 collided = false;
	TileMapPosition collision_position;
	if (!IsTileMapPointEmpty(tile_map, new_position))
	{
		collision_position = new_position;
		collided = true;
	}
	if (!IsTileMapPointEmpty(tile_map, position_left))
	{
		collision_position = position_left;
		collided = true;
	}
	if (!IsTileMapPointEmpty(tile_map, position_right))
	{
		collision_position = position_right;
		collided = true;
	}

	if (collided)
	{
		V2 reflection_normal = { 0, 1 };

		if (collision_position.tile_x < entity.position.tile_x)
		{
			reflection_normal = { 1, 0 };
		}
		if (collision_position.tile_x > entity.position.tile_x)
		{
			reflection_normal = { -1, 0 };
		}
		if (collision_position.tile_y < entity.position.tile_y)
		{
			reflection_normal = { 0, -1 };
		}
		if (collision_position.tile_y > entity.position.tile_y)
		{
			reflection_normal = { 0, 1 };
		}

		entity.velocity = entity.velocity - Inner(entity.velocity, reflection_normal) * reflection_normal;
	}
	else
	{
		entity.position = new_position;
	}
#else

#if 0
	u32 min_tile_x = Minimum(old_position.tile_x, new_position.tile_x);
	u32 min_tile_y = Minimum(old_position.tile_y, new_position.tile_y);
	u32 one_past_max_tile_x = Maximum(old_position.tile_x, new_position.tile_x) + 1;
	u32 one_past_max_tile_y = Maximum(old_position.tile_y, new_position.tile_y) + 1;
#else
	u32 start_tile_x = old_position.tile_x;
	u32 start_tile_y = old_position.tile_y;
	u32 end_tile_x = new_position.tile_x;
	u32 end_tile_y = new_position.tile_y;
	i32 delta_x = SignOf(static_cast<i32>(end_tile_x - start_tile_x));
	i32 delta_y = SignOf(static_cast<i32>(end_tile_y - start_tile_y));
#endif

	u32 tile_z = entity.position.tile_z;
	r32 min_time = 1.f;
	u32 tile_y = start_tile_y;
	for(;;)
	{
		u32 tile_x = start_tile_x;
		for (;;)
		{
			auto test_tile_position = CentredTilePoint(tile_x, tile_y, tile_z);
			u32 tile_value = GetTileValue(tile_map, test_tile_position);
			if (!IsTileValueEmpty(tile_value))
			{
				V2 min_corner = -0.5f * V2{ tile_map.tile_side_in_meters, tile_map.tile_side_in_meters };
				V2 max_corner = 0.5f * V2{ tile_map.tile_side_in_meters , tile_map.tile_side_in_meters };

				TileMapDifference relative_old_player_position = Subtract(tile_map, old_position, test_tile_position);
				V2 relative_position = relative_old_player_position.delta_xy;

				TestWall(min_corner.x, relative_position.x, relative_position.y, player_delta.x, player_delta.y, min_time, min_corner.y, max_corner.y);
				TestWall(max_corner.x, relative_position.x, relative_position.y, player_delta.x, player_delta.y, min_time, min_corner.y, max_corner.y);
				TestWall(min_corner.y, relative_position.y, relative_position.x, player_delta.y, player_delta.x, min_time, min_corner.x, max_corner.x);
				TestWall(max_corner.y, relative_position.y, relative_position.x, player_delta.y, player_delta.x, min_time, min_corner.x, max_corner.x);
			}
			if (tile_x == end_tile_x)
			{
				break;
			}
			else
			{
				tile_x = static_cast<u32>(static_cast<i32>(tile_x) + delta_x);
			}
		}
		if (tile_y == end_tile_y)
		{
			break;
		}
		else
		{
			tile_y = static_cast<u32>(static_cast<i32>(tile_y) + delta_y);
		}
	}

	entity.position = Offset(tile_map, old_position, min_time * player_delta);
#endif

	if (!IsSameTile(old_position, entity.position))
	{
		u32 tile_value = GetTileValue(tile_map, entity.position);
		if (tile_value == 3)
		{
			++entity.position.tile_z;
		}
		else if (tile_value == 4)
		{
			--entity.position.tile_z;
		}
	}

	if (entity.velocity.x == 0.f && entity.velocity.y == 0.f)
	{
		//Note: dont set facing when still
	}
	else if (AbsoluteValue(entity.velocity.x) > AbsoluteValue(entity.velocity.y))
	{
		if (entity.velocity.x > 0)
		{
			entity.facing_direction = 0;
		}
		else
		{
			entity.facing_direction = 2;
		}
	}
	else if (AbsoluteValue(entity.velocity.x) < AbsoluteValue(entity.velocity.y))
	{
		if (entity.velocity.y > 0)
		{
			entity.facing_direction = 1;
		}
		else
		{
			entity.facing_direction = 3;
		}
	}
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	u32 tiles_per_width = 17;
	u32 tiles_per_height = 9;

	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		//null entity index
		AddEntity(*game_state);

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
			Entity* controlling_entity = GetEntity(*game_state, game_state->player_index_for_controller[controller_index]);
			if (controlling_entity)
			{
				V2 player_acceleration{};

				if (controller.is_analog)
				{
					player_acceleration = { controller.stick_average_x, controller.stick_average_y };
				}
				else
				{

					if (controller.move_up.is_ended_down)
					{
						player_acceleration.y = 1.f;
					}
					if (controller.move_down.is_ended_down)
					{
						player_acceleration.y = -1.f;
					}
					if (controller.move_left.is_ended_down)
					{
						player_acceleration.x = -1.f;
					}
					if (controller.move_right.is_ended_down)
					{
						player_acceleration.x = 1.f;
					}
				}

				MovePlayer(*game_state, *controlling_entity, input->frame_delta, player_acceleration);
			}
			else
			{
				if (controller.start.is_ended_down)
				{
					u32 entity_index = AddEntity(*game_state);
					InitializePlayer(*game_state, entity_index);
					game_state->player_index_for_controller[controller_index] = entity_index;
				}
			}
		}
	}

	Entity* camera_following_entity = GetEntity(*game_state, game_state->camera_follow_entity_index);
	if (camera_following_entity)
	{
		game_state->camera_position.tile_z = camera_following_entity->position.tile_z;

		TileMapDifference difference = Subtract(tile_map, camera_following_entity->position, game_state->camera_position);
		if (difference.delta_xy.x > 9.f * tile_map.tile_side_in_meters)
		{
			game_state->camera_position.tile_x += tiles_per_width;
		}
		else if (difference.delta_xy.x < -9.f * tile_map.tile_side_in_meters)
		{
			game_state->camera_position.tile_x -= tiles_per_width;
		}
		if (difference.delta_xy.y > 5.f * tile_map.tile_side_in_meters)
		{
			game_state->camera_position.tile_y += tiles_per_height;
		}
		else if (difference.delta_xy.y < -5.f * tile_map.tile_side_in_meters)
		{
			game_state->camera_position.tile_y -= tiles_per_height;
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

				V2 v2_tile_side_in_pixels { r_tile_side_in_pixels * .5f,  r_tile_side_in_pixels * .5f };
				V2 center = {	screen_center_x - game_state->camera_position.offset_.x * meters_to_pixels + static_cast<r32>(relative_column) * r_tile_side_in_pixels,
								screen_center_y + game_state->camera_position.offset_.y * meters_to_pixels - static_cast<r32>(relative_row) * r_tile_side_in_pixels };
				V2 min = center - v2_tile_side_in_pixels;
				V2 max = center + v2_tile_side_in_pixels;

				DrawRectangle(buffer, min, max, shade, shade, shade);
			}
		}
	}

	Entity* entity = game_state->entities;
	for (u32 entity_index = 0; entity_index < game_state->entity_count; ++entity_index, ++entity)
	{
		if (entity->exists)
		{
			TileMapDifference difference = Subtract(tile_map, entity->position, game_state->camera_position);

			r32 player_gound_point_x = screen_center_x + meters_to_pixels * difference.delta_xy.x;
			r32 player_gound_point_y = screen_center_y - meters_to_pixels * difference.delta_xy.y;
			V2 player_left_top{
				player_gound_point_x - meters_to_pixels * (entity->width * .5f),
				player_gound_point_y - meters_to_pixels * (entity->height)
			};
			V2 player_width_height = { entity->width, entity->height };
			DrawRectangle(buffer, player_left_top, player_left_top + player_width_height * meters_to_pixels, 1.f, 0.f, 0.f);

			HeroBitmap& hero_bitmap = game_state->hero_bitmaps[entity->facing_direction];
			DrawBitmap(buffer, hero_bitmap.body, player_gound_point_x, player_gound_point_y, hero_bitmap.align_x, hero_bitmap.align_y);
			DrawBitmap(buffer, hero_bitmap.head, player_gound_point_x, player_gound_point_y, hero_bitmap.align_x, hero_bitmap.align_y);
		}
	}

	//App::Run();
}

extern "C"
ENGINE_API GAME_GET_SOUND_SAMPLES(PlatformGetSoundSamples)
{
	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	GameOutputSound(sound_buffer, 400);
}