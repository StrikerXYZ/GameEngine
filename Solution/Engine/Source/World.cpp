#include "World.hpp"

#include "Core.hpp"
#include "Intrinsics.hpp"
#include "EntryPoint.hpp"

struct MemoryArena;

constexpr i32 TILE_CHUNK_SAFE_MARGIN = INT32_MAX / 64;
constexpr i32 TILE_CHUNK_UNINITIALIZED = INT32_MAX;

internal_static void InitializeTileMap(World& tile_map, r32 tile_side_in_meters)
{
	//Note: using 16x16 tile chunks
	tile_map.chunk_shift = 4;
	tile_map.chunk_mask = (1 << tile_map.chunk_shift) - 1; //create mask be reverse shift and minus 1
	tile_map.chunk_dimension = (1 << tile_map.chunk_shift);

	tile_map.tile_side_in_meters = tile_side_in_meters;

	for (u32 tile_chunk_index = 0; tile_chunk_index < ArrayCount(tile_map.tile_chunk_hash); ++tile_chunk_index)
	{
		tile_map.tile_chunk_hash[tile_chunk_index].chunk_x = TILE_CHUNK_UNINITIALIZED;
	}
}
internal_static WorldChunk* GetTileChunk(World& tile_map, i32 tile_chunk_x, i32 tile_chunk_y, i32 tile_chunk_z, MemoryArena* arena = nullptr)
{
	Assert(tile_chunk_x > -TILE_CHUNK_SAFE_MARGIN || tile_chunk_y > -TILE_CHUNK_SAFE_MARGIN || tile_chunk_z > -TILE_CHUNK_SAFE_MARGIN);
	Assert(tile_chunk_x < TILE_CHUNK_SAFE_MARGIN || tile_chunk_y < TILE_CHUNK_SAFE_MARGIN || tile_chunk_z < TILE_CHUNK_SAFE_MARGIN);

	u32 hash_value = static_cast<u32>(19 * tile_chunk_x + 7 * tile_chunk_y + 3 * tile_chunk_z);
	u32 hash_slot = hash_value & (ArrayCount(tile_map.tile_chunk_hash) - 1);
	Assert(hash_slot < ArrayCount(tile_map.tile_chunk_hash));

	WorldChunk* chunk = tile_map.tile_chunk_hash + hash_slot;
	do
	{
		if (tile_chunk_x == chunk->chunk_x &&
			tile_chunk_y == chunk->chunk_y &&
			tile_chunk_z == chunk->chunk_z)
		{
			break;
		}

		if (arena && chunk->chunk_x != TILE_CHUNK_UNINITIALIZED && !chunk->next_in_hash)
		{
			chunk->next_in_hash = PushStruct(*arena, WorldChunk);
			chunk = chunk->next_in_hash;
			chunk->chunk_x = TILE_CHUNK_UNINITIALIZED;
		}

		if (arena && chunk->chunk_x == TILE_CHUNK_UNINITIALIZED)
		{
			i32 tile_count = tile_map.chunk_dimension * tile_map.chunk_dimension;

			chunk->chunk_x = tile_chunk_x;
			chunk->chunk_y = tile_chunk_y;
			chunk->chunk_z = tile_chunk_z;

			chunk->next_in_hash = nullptr;

			break;
		}

		chunk = chunk->next_in_hash;
	} while (chunk);

	return chunk;
}

#if 0
internal_static u32 GetTileValueUnchecked(TileMap& tile_map, TileChunk& tile_chunk, i32 tile_x, i32 tile_y)
{
	Assert(tile_x < tile_map.chunk_dimension);
	Assert(tile_y < tile_map.chunk_dimension);
	return tile_chunk.tiles[static_cast<i32>(tile_y) * tile_map.chunk_dimension + static_cast<i32>(tile_x)];
}

internal_static void SetTileValueUnchecked(TileMap& tile_map, TileChunk& tile_chunk, i32 tile_x, i32 tile_y, u32 value)
{
	Assert(tile_x < tile_map.chunk_dimension);
	Assert(tile_y < tile_map.chunk_dimension);
	tile_chunk.tiles[static_cast<i32>(tile_y) * tile_map.chunk_dimension + static_cast<i32>(tile_x)] = value;
}

internal_static TileChunkPosition GetChunkPosition(World& tile_map, i32 x, i32 y, i32 z)
{
	TileChunkPosition result;
	result.chunk_x = x >> tile_map.chunk_shift;
	result.chunk_y = y >> tile_map.chunk_shift;
	result.chunk_z = z;
	result.chunk_tile_x = x & tile_map.chunk_mask;
	result.chunk_tile_y = y & tile_map.chunk_mask;
	return result;
}

internal_static u32 GetTileValue(TileMap& tile_map, i32 tile_x, i32 tile_y, i32 tile_z)
{
	TileChunkPosition chunk_position = GetChunkPosition(tile_map, tile_x, tile_y, tile_z);
	TileChunk* tile_chunk = GetTileChunk(tile_map, chunk_position.chunk_x, chunk_position.chunk_y, chunk_position.chunk_z);
	return (tile_chunk && tile_chunk->tiles) ? GetTileValueUnchecked(tile_map, *tile_chunk, chunk_position.chunk_tile_x, chunk_position.chunk_tile_y) : 0;
}

internal_static u32 GetTileValue(TileMap& tile_map, TileMapPosition position)
{
	u32 result = GetTileValue(tile_map, position.tile_x, position.tile_y, position.tile_z);
	return result;
}

internal_static void SetTileValue(TileMap& tile_map, i32 tile_x, i32 tile_y, i32 tile_z, u32 value)
{
	TileChunkPosition chunk_position = GetChunkPosition(tile_map, tile_x, tile_y, tile_z);
	TileChunk* tile_chunk = GetTileChunk(tile_map, chunk_position.chunk_x, chunk_position.chunk_y, chunk_position.chunk_z);
	if (tile_chunk)
	{
		SetTileValueUnchecked(tile_map, *tile_chunk, chunk_position.chunk_tile_x, chunk_position.chunk_tile_y, value);
	}
}

internal_static void SetTileValue(MemoryArena& arena, TileMap tile_map, i32 abs_tile_x, i32 abs_tile_y, i32 abs_tile_z, u32 value)
{
	TileChunkPosition chunk_position = GetChunkPosition(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
	TileChunk* tile_chunk = GetTileChunk(tile_map, chunk_position.chunk_x, chunk_position.chunk_y, chunk_position.chunk_z, &arena);

	Assert(tile_chunk);

	if (!tile_chunk->tiles)
	{
		i32 tile_count = tile_map.chunk_dimension * tile_map.chunk_dimension;
		tile_chunk->tiles = PushArray(arena, static_cast<MemoryIndex>(tile_count), u32);
		for (i32 tile_index = 0; tile_index < tile_count; ++tile_index)
		{
			tile_chunk->tiles[tile_index] = 1u;
		}
	}
	SetTileValueUnchecked(tile_map, *tile_chunk, chunk_position.chunk_tile_x, chunk_position.chunk_tile_y, value);
}

internal_static b32 IsTileValueEmpty(u32 tile_value)
{
	b32 is_empty = (tile_value == 1 || tile_value == 3 || tile_value == 4);
	return is_empty;
}

internal_static b32 IsTileMapPointEmpty(TileMap& tile_map, TileMapPosition position)
{
	u32 tile_value = GetTileValue(tile_map, position);
	b32 is_empty = IsTileValueEmpty(tile_value);
	return is_empty;
}
#endif

inline void RecanonicalizeCoord(World& tile_map, i32& tile, r32& relative)
{
	r32 tile_side_in_meters = static_cast<r32>(tile_map.tile_side_in_meters);

	//Note: TileMap is assumed to be toroidal topology
	i32 offset = RoundToI32(relative / tile_side_in_meters);
	tile = static_cast<i32>(tile) + offset;
	relative -= static_cast<r32>(offset) * tile_side_in_meters;

	Assert(relative >= tile_side_in_meters * -.5f);
	Assert(relative <= tile_side_in_meters * .5f);
}

inline WorldPosition MapIntoTileSpace(World& tile_map, WorldPosition base_position, V2 offset)
{
	WorldPosition result = base_position;

	result.offset_ += offset;
	RecanonicalizeCoord(tile_map, result.tile_x, result.offset_.x);
	RecanonicalizeCoord(tile_map, result.tile_y, result.offset_.y);

	return result;
}

inline b32 IsSameTile(WorldPosition a, WorldPosition b)
{
	b32 result = (a.tile_x == b.tile_x &&
		a.tile_y == b.tile_y &&
		a.tile_z == b.tile_z);
	return result;
}

inline WorldDifference Subtract(World& tile_map, WorldPosition& a, WorldPosition& b)
{
	WorldDifference result;

	V2 delta_tile_xy	{	
							(static_cast<float>(a.tile_x) - static_cast<float>(b.tile_x)), 
							(static_cast<float>(a.tile_y) - static_cast<float>(b.tile_y)) 
						};
	r32 delta_tile_z = static_cast<float>(a.tile_y) - static_cast<float>(b.tile_z);

	result.delta_xy = tile_map.tile_side_in_meters * delta_tile_xy + a.offset_ - b.offset_;
	result.delta_z = tile_map.tile_side_in_meters * delta_tile_z;

	return result;
}

inline WorldPosition CentredTilePoint(i32 tile_x, i32 tile_y, i32 tile_z)
{
	WorldPosition result{};
	result.tile_x = tile_x;
	result.tile_y = tile_y;
	result.tile_z = tile_z;

	return result;
}