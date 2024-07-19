#include "EntryPoint.hpp"

#include "Definition.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "Intrinsics.hpp"
#include "World.hpp"
#include "Random.hpp"
#include "Math.hpp"

#include "World.cpp"

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

void DrawBitmap(const GameOffscreenBuffer& buffer, LoadedBitmap& bitmap, r32 in_x, r32 in_y, i32 align_x = 0, i32 align_y = 0, r32 c_alpha = 1.f)
{
	in_x -= static_cast<float>(align_x);
	in_y -= static_cast<float>(align_y);

	i32 imin_x = RoundToI32(in_x);
	i32 imin_y = RoundToI32(in_y);
	i32 imax_x = static_cast<i32>(in_x + static_cast<r32>(bitmap.width));
	i32 imax_y = static_cast<i32>(in_y + static_cast<r32>(bitmap.height));

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
			alpha *= c_alpha;

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

internal_static LowEntity* GetLowEntity(GameState& game_state, u32 index)
{
	LowEntity* result{};

	if (index > 0 && index < game_state.low_entity_count)
	{
		result = game_state.low_entities + index;
	}

	return result;
}

inline HighEntity* MakeEntityHighFrequency(GameState& game_state, u32 low_index)
{
	HighEntity* high_entity{};

	LowEntity& low_entity = game_state.low_entities[low_index];
	if (low_entity.high_entity_index)
	{
		high_entity = game_state.high_entities + low_entity.high_entity_index;
	}
	else
	{
		if (game_state.high_entity_count < ArrayCount(game_state.high_entities))
		{
			u32 high_index = game_state.high_entity_count++;
			high_entity = &game_state.high_entities[high_index];

			WorldDifference difference = Subtract(*(game_state.world), low_entity.position, game_state.camera_position);
			high_entity->position = difference.delta_xy;
			high_entity->velocity = {};
			high_entity->tile_z = low_entity.position.tile_z;
			high_entity->facing_direction = 0;
			high_entity->low_entity_index = low_index;

			low_entity.high_entity_index = high_index;
		}
		else
		{
			InvalidCodePath();
		}
	}

	return high_entity;
}

inline void MakeEntityLowFrequency(GameState& game_state, u32 low_index)
{
	LowEntity& low_entity = game_state.low_entities[low_index];
	u32 high_index = low_entity.high_entity_index;
	if (high_index)
	{
		u32 last_high_index = game_state.high_entity_count - 1;
		if (high_index != last_high_index)
		{
			HighEntity& last_high_entity = game_state.high_entities[last_high_index];
			HighEntity& deleted_high_entity = game_state.high_entities[high_index];

			deleted_high_entity = last_high_entity;
			game_state.low_entities[last_high_entity.low_entity_index].high_entity_index = 0;
		}
		--high_index;
		game_state.low_entities[low_index].high_entity_index = 0;
	}
}

internal_static Entity GetHighEntity(GameState& game_state, u32 low_index)
{
	Entity result{};

	if (low_index > 0 && low_index < game_state.low_entity_count)
	{
		result.low_index = low_index;
		result.low = game_state.low_entities + low_index;
		result.high = MakeEntityHighFrequency(game_state, low_index);
	}

	return result;
}

inline void OffsetAndCheckFrequencyByArea(GameState& game_state, V2 offset, Rectangle high_frequency_bounds)
{
	for (u32 high_index = 0; high_index < game_state.high_entity_count;)
	{
		HighEntity& high_entity = game_state.high_entities[high_index];
		high_entity.position += offset;

		if (IsInRectangle(high_frequency_bounds, high_entity.position))
		{
			++high_index;
		}
		else
		{
			MakeEntityLowFrequency(game_state, high_entity.low_entity_index);
		}
	}
}

internal_static u32 AddLowEntity(GameState& game_state, EntityType type)
{
	Assert(game_state.low_entity_count < ArrayCount(game_state.low_entities));
	u32 entity_index = game_state.low_entity_count++;

	game_state.low_entities[entity_index] = {};
	game_state.low_entities[entity_index].type = type;

	return entity_index;
}

internal_static u32 AddWall(GameState& game_state, i32 tile_x, i32 tile_y, i32 tile_z)
{
	u32 low_index = AddLowEntity(game_state, EntityType_Wall);
	LowEntity* entity = GetLowEntity(game_state, low_index);
	entity->position.tile_x = tile_x;
	entity->position.tile_y = tile_y;
	entity->position.tile_z = tile_z;
	entity->height = game_state.world->tile_side_in_meters;
	entity->width = entity->height;
	entity->collides = true;

	return low_index;
}

internal_static u32 AddPlayer(GameState& game_state)
{
	u32 low_index = AddLowEntity(game_state, EntityType_Hero);
	LowEntity* entity = GetLowEntity(game_state, low_index);
	entity->position.tile_x = 2;
	entity->position.tile_y = 2;
	entity->position.offset_ = { .5f , .5f };
	entity->position = game_state.camera_position;
	entity->height = 0.5f;//1.4f;
	entity->width = 1.0f;// entity->height * 0.75f;
	entity->collides = true;

	//MakeEntityHighFrequency(game_state, low_index);

	if (game_state.camera_follow_entity_index == 0)
	{
		game_state.camera_follow_entity_index = low_index;
	}

	return low_index;
}

internal_static b32 TestWall(r32 wall_x, r32 relative_x, r32 relative_y, r32 player_delta_x, r32 player_delta_y, r32& min_time, r32 min_y, r32 max_y)
{
	b32 hit = false;

	r32 time_epsilon = 0.001f;
	if (player_delta_x != 0.f)
	{
		r32 result_time = (wall_x - relative_x) / player_delta_x;
		r32 y = relative_y + result_time * player_delta_y;
		if (result_time >= 0.f && result_time < min_time)
		{
			if (y >= min_y && y <= max_y)
			{
				min_time = Maximum(0.f, result_time - time_epsilon);
				hit = true;
			}
		}
	}

	return hit;
}

internal_static void MovePlayer(GameState& game_state, Entity& entity, r32 delta_time, V2 acceleration)
{
	World& world = *game_state.world;

	auto acceleration_length_squared = LengthSquared(acceleration);
	if (acceleration_length_squared > 1.0f)
	{
		//get unit vector
		acceleration *= OneOverSquareRoot(acceleration_length_squared);
	}

	r32 player_speed = 50.f; //m/s^2

	acceleration *= player_speed;

	acceleration -= 8.0f * entity.high->velocity;

	V2 old_position = entity.high->position;

	//p' = 1/2 at^2 + vt + p
	V2 player_delta = 0.5f * acceleration * Square(delta_time) + entity.high->velocity * delta_time;

	//v' = at + v
	entity.high->velocity = acceleration * delta_time + entity.high->velocity;

	V2 new_position = old_position + player_delta;

		/*u32 min_tile_x = Minimum(old_position.x, new_position.x);
	u32 min_tile_y = Minimum(old_position.y, new_position.y);
	u32 max_tile_x = Maximum(old_position.x, new_position.x);
	u32 max_tile_y = Maximum(old_position.y, new_position.y);

	u32 entity_tile_width = CeilToU32(entity.dormant->width / world.tile_side_in_meters);
	u32 entity_tile_height = CeilToU32(entity.dormant->height / world.tile_side_in_meters);

	min_tile_x -= entity_tile_width;
	min_tile_y -= entity_tile_height;
	max_tile_x += entity_tile_width;
	max_tile_y += entity_tile_height;

	u32 tile_z = entity.dormant->position.tile_z;*/

	//r32 remaining_time = 1.0f;
	for (u32 iteration = 0; iteration < 4; ++iteration)
	{
		r32 min_time = 1.0f;

		V2 wall_normal{};

		u32 hit_high_entity_index = 0;

		V2 desired_position = entity.high->position + player_delta;

		for (u32 test_high_index = 1; test_high_index < game_state.high_entity_count; ++test_high_index)
		{
			if (test_high_index != entity.low->high_entity_index)
			{
				Entity test_entity;
				test_entity.high = game_state.high_entities + test_high_index;
				test_entity.low_index = test_entity.high->low_entity_index;
				test_entity.low = game_state.low_entities + test_entity.low_index;
				if (test_entity.low->collides)
				{
					r32 diameter_width = test_entity.low->width + entity.low->width;
					r32 diameter_height = test_entity.low->width + entity.low->height;

					V2 min_corner = -0.5f * V2{ diameter_width, diameter_height };
					V2 max_corner = 0.5f * V2{ diameter_width, diameter_height };

					V2 relative_position = entity.high->position - test_entity.high->position;

					if (TestWall(min_corner.x, relative_position.x, relative_position.y, player_delta.x, player_delta.y, min_time, min_corner.y, max_corner.y))
					{
						wall_normal = V2{ -1, 0 };
						hit_high_entity_index = test_high_index;
					}
					if (TestWall(max_corner.x, relative_position.x, relative_position.y, player_delta.x, player_delta.y, min_time, min_corner.y, max_corner.y))
					{
						wall_normal = V2{ 1, 0 };
						hit_high_entity_index = test_high_index;
					}
					if (TestWall(min_corner.y, relative_position.y, relative_position.x, player_delta.y, player_delta.x, min_time, min_corner.x, max_corner.x))
					{
						wall_normal = V2{ 0, -1 };
						hit_high_entity_index = test_high_index;
					}
					if (TestWall(max_corner.y, relative_position.y, relative_position.x, player_delta.y, player_delta.x, min_time, min_corner.x, max_corner.x))
					{
						wall_normal = V2{ 0, 1 };
						hit_high_entity_index = test_high_index;
					}
				}
			}
		}

		entity.high->position += min_time * player_delta;
		if (hit_high_entity_index)
		{
			entity.high->velocity = entity.high->velocity - Inner(entity.high->velocity, wall_normal) * wall_normal;
			player_delta = desired_position - entity.high->position;
			player_delta = player_delta - Inner(player_delta, wall_normal) * wall_normal;

			Entity hit_entity = GetHighEntity(game_state, hit_high_entity_index);
			entity.high->tile_z += static_cast<u32>(static_cast<i32>(entity.high->tile_z) + hit_entity.low->velocity_tile_z);
		}
		else
		{
			break;
		}

	}

	if (entity.high->velocity.x == 0.f && entity.high->velocity.y == 0.f)
	{
		//Note: dont set facing when still
	}
	else if (AbsoluteValue(entity.high->velocity.x) > AbsoluteValue(entity.high->velocity.y))
	{
		if (entity.high->velocity.x > 0)
		{
			entity.high->facing_direction = 0;
		}
		else
		{
			entity.high->facing_direction = 2;
		}
	}
	else if (AbsoluteValue(entity.high->velocity.x) < AbsoluteValue(entity.high->velocity.y))
	{
		if (entity.high->velocity.y > 0)
		{
			entity.high->facing_direction = 1;
		}
		else
		{
			entity.high->facing_direction = 3;
		}
	}

	entity.low->position = MapIntoTileSpace(world, game_state.camera_position, entity.high->position);
}

internal_static void SetCamera(GameState& game_state, WorldPosition new_camera_position)
{
	World& world = *(game_state.world);

	WorldDifference delta_camera_position = Subtract(world, new_camera_position, game_state.camera_position);
	game_state.camera_position = new_camera_position;

	i32 tile_span_x = 17 * 3;
	i32 tile_span_y = 9 * 3;
	Rectangle camera_in_bounds = RectCenterDim(V2{}, V2{ static_cast<r32>(tile_span_x), static_cast<r32>(tile_span_y) } * world.tile_side_in_meters);
	V2 entity_offset_for_frame = -delta_camera_position.delta_xy;
	OffsetAndCheckFrequencyByArea(game_state, entity_offset_for_frame, camera_in_bounds);

	i32 min_tile_x = new_camera_position.tile_x - tile_span_x / 2;
	i32 min_tile_y = new_camera_position.tile_y - tile_span_y / 2;
	i32 max_tile_x = new_camera_position.tile_x + tile_span_x / 2;
	i32 max_tile_y = new_camera_position.tile_y + tile_span_y / 2;

	for (u32 entity_index = 1; entity_index < game_state.low_entity_count; ++entity_index)
	{
		LowEntity& low_entity = game_state.low_entities[entity_index];
		
		if (low_entity.high_entity_index == 0)
		{
			if (low_entity.position.tile_z == new_camera_position.tile_z &&
				low_entity.position.tile_x >= min_tile_x &&
				low_entity.position.tile_x <= max_tile_x &&
				low_entity.position.tile_y >= min_tile_y &&
				low_entity.position.tile_y <= max_tile_y)
			{
				MakeEntityHighFrequency(game_state, entity_index);
			}
		}
	}
}

extern "C"
ENGINE_API GAME_LOOP(PlatformLoop)
{
	i32 tiles_per_width = 17;
	i32 tiles_per_height = 9;

	GameState* game_state = reinterpret_cast<GameState*>(memory->permanent_storage);
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	if (!memory->is_initialized)
	{
		//null entity index
		AddLowEntity(*game_state, EntityType_Null);
		game_state->high_entity_count = 1;

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

		InitializeArena(game_state->world_arena, 
			memory->permanent_storage_size - sizeof(GameState),
			static_cast<u8*>(memory->permanent_storage) + sizeof(GameState));

		game_state->world = PushStruct(game_state->world_arena, World);
		World& world = *game_state->world;

		InitializeTileMap(world, 1.4f);

		i32 screen_base_x = 0;
		i32 screen_base_y = 0;
		i32 screen_base_z = 0;
		i32 screen_x = screen_base_x;
		i32 screen_y = screen_base_y;
		i32 abs_tile_z = screen_base_z;

		u32 random_number_index = 0;

		b32 door_left = false;
		b32 door_top = false;
		b32 door_right = false;
		b32 door_bottom = false;
		b32 door_up = false;
		b32 door_down = false;

		for (u32 screen_index = 0; screen_index < 2; ++screen_index)
		{
			Assert(random_number_index < ArrayCount(random_number_table));
			u32 random_choice;
			/*if (door_up || door_down)
			{*/
				random_choice = random_number_table[random_number_index++] % 2;
			/*}
			else
			{
				random_choice = random_number_table[random_number_index++] % 3;
			}*/

			b32 created_z_door = false;
			if (random_choice == 2)
			{
				created_z_door = true;
				if (abs_tile_z == screen_base_z)
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

			for (i32 tile_y = 0; tile_y < tiles_per_height; ++tile_y)
			{
				for (i32 tile_x = 0; tile_x < tiles_per_width; ++tile_x)
				{
					i32 abs_tile_x = screen_x * tiles_per_width + tile_x;
					i32 abs_tile_y = screen_y * tiles_per_height + tile_y;

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

					if (tile_value == 2)
					{
						AddWall(*game_state, abs_tile_x, abs_tile_y, abs_tile_z);
					}
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
				if (abs_tile_z == screen_base_z)
				{
					abs_tile_z = screen_base_z + 1;
				}
				else
				{
					abs_tile_z = screen_base_z;
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

#if 0
		while (game_state->low_entity_count < ArrayCount(game_state->low_entities) - 16)
		{
			i32 coords = 1024 + static_cast<i32>(game_state->low_entity_count);
			AddWall(*game_state, coords, coords, coords);
		}
#endif

		WorldPosition new_camera_position{};
		new_camera_position.tile_x = screen_base_x + tiles_per_width / 2;
		new_camera_position.tile_y = screen_base_y + tiles_per_height / 2;
		new_camera_position.tile_z = screen_base_z;
		SetCamera(*game_state, new_camera_position);

		memory->is_initialized = true;
	}

	World& world = *(game_state->world);

	i32 tile_side_in_pixels = 60;
	r32 r_tile_side_in_pixels = static_cast<r32>(tile_side_in_pixels);
	r32 meters_to_pixels = static_cast<r32>(tile_side_in_pixels) / world.tile_side_in_meters;

	r32 lower_left_x = -r_tile_side_in_pixels/2;
	r32 lower_left_y = static_cast<r32>(buffer.height);

	for (u64 controller_index = 0; controller_index < ArrayCount(input->controllers); ++controller_index)
	{
		const GameControllerInput& controller = input->controllers[controller_index];
		{
			u32 low_index = game_state->player_index_for_controller[controller_index];
			if (low_index == 0)
			{
				if (controller.start.is_ended_down)
				{
					u32 entity_index = AddPlayer(*game_state);
					game_state->player_index_for_controller[controller_index] = entity_index;
				}
			}
			else
			{
				Entity controlling_entity = GetHighEntity(*game_state, low_index);

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
				if (controller.action_up.is_ended_down)
				{
					controlling_entity.high->delta_z = 3.f;
				}

				MovePlayer(*game_state, controlling_entity, input->frame_delta, player_acceleration);
			}
		}
	}

	Entity camera_following_entity = GetHighEntity(*game_state, game_state->camera_follow_entity_index);
	if (camera_following_entity.high)
	{
		WorldPosition new_camera_position = game_state->camera_position;

		new_camera_position.tile_z = camera_following_entity.low->position.tile_z;

#if 1 //no scrolling cam
		if (camera_following_entity.high->position.x > 9.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_x += tiles_per_width;
		}
		else if (camera_following_entity.high->position.x < -9.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_x -= tiles_per_width;
		}
		if (camera_following_entity.high->position.y > 5.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_y += tiles_per_height;
		}
		else if (camera_following_entity.high->position.y < -5.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_y -= tiles_per_height;
		}
#else	//scrolling cam
		if (camera_following_entity.high->position.x > 1.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_x += 1;
		}
		else if (camera_following_entity.high->position.x < -1.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_x -= 1;
		}
		if (camera_following_entity.high->position.y > 1.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_y += 1;
		}
		else if (camera_following_entity.high->position.y < -1.f * world.tile_side_in_meters)
		{
			new_camera_position.tile_y -= 1;
		}
#endif

		SetCamera(*game_state, new_camera_position);
	}

	DrawBitmap(buffer, game_state->backdrop, 0.f, 0.f);

	r32 screen_center_x = .5f * static_cast<r32>(buffer.width);
	r32 screen_center_y = .5f * static_cast<r32>(buffer.height);

#if 0
	for (i32 relative_row = -10; relative_row < 10; ++relative_row)
	{
		for (i32 relative_column = -20; relative_column < 20; ++relative_column)
		{
			i32 signed_column = relative_column + static_cast<i32>(game_state->camera_position.tile_x);
			i32 signed_row = relative_row + static_cast<i32>(game_state->camera_position.tile_y);

			u32 column = static_cast<u32>(signed_column);
			u32 row = static_cast<u32>(signed_row);

			u32 tile_id = GetTileValue(world, column, row, game_state->camera_position.tile_z);

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
#endif

	for (u32 high_index = 0; high_index < game_state->high_entity_count; ++high_index)
	{
			HighEntity& high_entity = game_state->high_entities[high_index];
			LowEntity& low_entity = game_state->low_entities[high_entity.low_entity_index];

			//high_entity.position += entity_offset_for_frame;

			r32 dt = input->frame_delta;
			r32 gravity = -9.8f;
			high_entity.z += 0.5f * gravity * Square(dt) + high_entity.delta_z * dt;
			high_entity.delta_z += gravity * dt;
			if (high_entity.z < 0)
			{
				high_entity.z = 0;
			}

			r32 player_gound_point_x = screen_center_x + meters_to_pixels * high_entity.position.x;
			r32 player_gound_point_y = screen_center_y - meters_to_pixels * high_entity.position.y;
			r32 z = -meters_to_pixels * high_entity.z;
			V2 player_left_top{
				player_gound_point_x - meters_to_pixels * (low_entity.width * .5f),
				player_gound_point_y - meters_to_pixels * (low_entity.height * .5f)
			};
			V2 player_width_height = { low_entity.width, low_entity.height };

			if (low_entity.type == EntityType_Hero)
			{
				HeroBitmap& hero_bitmap = game_state->hero_bitmaps[high_entity.facing_direction];
				DrawBitmap(buffer, hero_bitmap.body, player_gound_point_x, player_gound_point_y + z, hero_bitmap.align_x, hero_bitmap.align_y);
				DrawBitmap(buffer, hero_bitmap.head, player_gound_point_x, player_gound_point_y + z, hero_bitmap.align_x, hero_bitmap.align_y);
				DrawRectangle(buffer, player_left_top, player_left_top + player_width_height * meters_to_pixels, 1.f, 0.f, 0.f);
			}
			else
			{
				DrawRectangle(buffer, player_left_top, player_left_top + player_width_height * meters_to_pixels, 1.f, 1.f, 1.f);
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