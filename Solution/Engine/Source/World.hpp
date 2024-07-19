#pragma once

#include "Definition.hpp"
#include "Math.hpp"


struct WorldPosition
{
	i32 tile_x;
	i32 tile_y;
	i32 tile_z;
	V2 offset_;
};

//struct TileChunkPosition
//{
//	i32 chunk_x;
//	i32 chunk_y;
//	i32 chunk_z;
//
//	i32 chunk_tile_x;
//	i32 chunk_tile_y;
//};

struct WorldEntityBlock
{
	u32 entity_count;
	u32 low_entity_index[16];
	WorldEntityBlock* next;
};

struct WorldChunk
{
	i32 chunk_x;
	i32 chunk_y;
	i32 chunk_z;

	WorldEntityBlock first_block;

	WorldChunk* next_in_hash;
};

struct World
{
	r32 tile_side_in_meters;

	i32 chunk_shift;
	i32 chunk_mask;
	i32 chunk_dimension;
	WorldChunk tile_chunk_hash[4096];
};

struct WorldDifference
{
	V2 delta_xy;
	r32 delta_z;
};