#include <block/pos.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	pos_type PosConvetrer::globalPosToRegionPos(pos_type pos)
	{
		return pos / BlockMapRegion::block_size;
	}

	pos_type PosConvetrer::globalPosToInChunkPos(pos_type pos)
	{
		return mod(pos, BlockMapRegion::block_size);
	}

	pos_type PosConvetrer::chunkPosToRegionOffset(pos_type pos)
	{
		return pos / BlockMapRegion::size;
	}

	pos_type PosConvetrer::validateChunkPos(pos_type block_pos)
	{
		return mod(block_pos, BlockMapRegion::size);
	}

	pos_type PosConvetrer::regionPosToChunkPos(pos_type pos)
	{
		return pos_type(pos.x / BlockMapChunk::size.x, pos.y / BlockMapChunk::size.y, pos.z / BlockMapChunk::size.z);
	}

	pos_type PosConvetrer::regionPosToBlockPos(pos_type pos)
	{
		return pos_type(pos.x % BlockMapChunk::size.x, pos.y % BlockMapChunk::size.y, pos.z % BlockMapChunk::size.z);
	}

	pos_type PosConvetrer::blockPosToChunkOffset(pos_type block_pos)
	{
		pos_type chunk_offset = block_pos / BlockMapChunk::size;
		return chunk_offset;
	}

	pos_type PosConvetrer::validateBlockPos(pos_type block_pos)
	{
		return mod(block_pos, BlockMapChunk::size);
	}

	vec3 PosConvetrer::chunkPosToChunkCenterCoord(pos_type chunk_pos)
	{
		return current_block_context->region->pos * BlockMapRegion::block_size + chunk_pos * BlockMapChunk::size;
	}

	pos_type PosConvetrer::worldPosToGlobalCoord(vec3 pos)
	{
		return pos;
	}
}
