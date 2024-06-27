#include "Tile.hpp"

#include "Core.hpp"
#include "Intrinsics.hpp"
#include "EntryPoint.hpp"

struct MemoryArena;

internal_static TileChunk* GetTileChunk(TileMap& tile_map, u32 tile_chunk_x, u32 tile_chunk_y, u32 tile_chunk_z)
{
	TileChunk* tile_chunk = nullptr;

	if (tile_chunk_x < tile_map.count_x &&
		tile_chunk_y < tile_map.count_y &&
		tile_chunk_z < tile_map.count_z)
	{
		tile_chunk = &tile_map.tile_chunks[
			tile_chunk_z * tile_map.count_y * tile_map.count_x +
			tile_chunk_y * tile_map.count_x +
			tile_chunk_x
		];
	}

	return tile_chunk;
}

internal_static u32 GetTileValueUnchecked(TileMap& tile_map, TileChunk& tile_chunk, u32 tile_x, u32 tile_y)
{
	Assert(tile_x < static_cast<u32>(tile_map.chunk_dimension));
	Assert(tile_y < static_cast<u32>(tile_map.chunk_dimension));
	return tile_chunk.tiles[static_cast<i32>(tile_y) * tile_map.chunk_dimension + static_cast<i32>(tile_x)];
}

internal_static void SetTileValueUnchecked(TileMap& tile_map, TileChunk& tile_chunk, u32 tile_x, u32 tile_y, u32 value)
{
	Assert(tile_x < static_cast<u32>(tile_map.chunk_dimension));
	Assert(tile_y < static_cast<u32>(tile_map.chunk_dimension));
	tile_chunk.tiles[static_cast<i32>(tile_y) * tile_map.chunk_dimension + static_cast<i32>(tile_x)] = value;
}

internal_static TileChunkPosition GetChunkPosition(TileMap& tile_map, u32 x, u32 y, u32 z)
{
	TileChunkPosition result;
	result.chunk_x = x >> tile_map.chunk_shift;
	result.chunk_y = y >> tile_map.chunk_shift;
	result.chunk_z = z;
	result.chunk_tile_x = x & tile_map.chunk_mask;
	result.chunk_tile_y = y & tile_map.chunk_mask;
	return result;
}

internal_static u32 GetTileValue(TileMap& tile_map, u32 tile_x, u32 tile_y, u32 tile_z)
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

internal_static void SetTileValue(TileMap& tile_map, u32 tile_x, u32 tile_y, u32 tile_z, u32 value)
{
	TileChunkPosition chunk_position = GetChunkPosition(tile_map, tile_x, tile_y, tile_z);
	TileChunk* tile_chunk = GetTileChunk(tile_map, chunk_position.chunk_x, chunk_position.chunk_y, chunk_position.chunk_z);
	if (tile_chunk)
	{
		SetTileValueUnchecked(tile_map, *tile_chunk, chunk_position.chunk_tile_x, chunk_position.chunk_tile_y, value);
	}
}

internal_static b32 IsTileMapPointEmpty(TileMap& tile_map, TileMapPosition position)
{
	u32 tile_value = GetTileValue(tile_map, position);
	b32 is_empty = (tile_value == 1 || tile_value == 3 || tile_value == 4);
	return is_empty;
}

internal_static void SetTileValue(MemoryArena& arena, TileMap tile_map, u32 abs_tile_x, u32 abs_tile_y, u32 abs_tile_z, u32 value)
{
	TileChunkPosition chunk_position = GetChunkPosition(tile_map, abs_tile_x, abs_tile_y, abs_tile_z);
	TileChunk* tile_chunk = GetTileChunk(tile_map, chunk_position.chunk_x, chunk_position.chunk_y, chunk_position.chunk_z);
	
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

///

inline void RecanonicalizeCoord(TileMap& tile_map, u32& tile, r32& relative)
{
	r32 tile_side_in_meters = static_cast<r32>(tile_map.tile_side_in_meters);

	//Note: TileMap is assumed to be toroidal topology
	i32 offset = RoundToI32(relative / tile_side_in_meters);
	tile = static_cast<u32>(static_cast<i32>(tile) + offset);
	relative -= static_cast<r32>(offset) * tile_side_in_meters;

	Assert(relative >= tile_side_in_meters * -.5f);
	Assert(relative <= tile_side_in_meters * .5f);
}

inline TileMapPosition RecanonicalizePosition(TileMap& tile_map, TileMapPosition position)
{
	TileMapPosition canonical_position = position;

	RecanonicalizeCoord(tile_map, canonical_position.tile_x, canonical_position.offset_x);
	RecanonicalizeCoord(tile_map, canonical_position.tile_y, canonical_position.offset_y);

	return canonical_position;
}

inline b32 IsSameTile(TileMapPosition a, TileMapPosition b)
{
	b32 result = (a.tile_x == b.tile_x &&
		a.tile_y == b.tile_y &&
		a.tile_z == b.tile_z);
	return result;
}

inline TileMapDifference Subtract(TileMap& tile_map, TileMapPosition& a, TileMapPosition& b)
{
	TileMapDifference result;

	r32 delta_tile_x = static_cast<float>(a.tile_x) - static_cast<float>(b.tile_x);
	r32 delta_tile_y = static_cast<float>(a.tile_y) - static_cast<float>(b.tile_y);
	r32 delta_tile_z = static_cast<float>(a.tile_y) - static_cast<float>(b.tile_z);

	result.delta_x = tile_map.tile_side_in_meters * delta_tile_x + a.offset_x - b.offset_x;
	result.delta_y = tile_map.tile_side_in_meters * delta_tile_y + a.offset_y - b.offset_y;

	result.delta_z = tile_map.tile_side_in_meters * delta_tile_z;

	return result;
}