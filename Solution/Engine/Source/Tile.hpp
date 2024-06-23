#pragma once

#include "Definition.hpp"

struct TileChunkPosition
{
	u32 chunk_x;
	u32 chunk_y;
	u32 chunk_z;

	u32 chunk_tile_x;
	u32 chunk_tile_y;
};

struct TileChunk
{
	u32* tiles;
};

struct TileMap
{
	u32 chunk_shift;
	u32 chunk_mask;
	i32 chunk_dimension;

	r32 tile_side_in_meters;

	u32 count_x;
	u32 count_y;
	u32 count_z;
	TileChunk* tile_chunks;
};

struct TileMapPosition
{
	u32 tile_x;
	u32 tile_y;
	u32 tile_z;
	r32 offset_x;
	r32 offset_y;
};