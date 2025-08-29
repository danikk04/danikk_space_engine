#include <preset/block/base.h>
#include <preset/block/inserter.h>
#include <block/container.h>

namespace danikk_space_engine
{
	void fillRegion(BlockMapRegion& target, const BlockSlot& block)
	{
		for(BlockMapChunk& chunk : target)
		{
			chunk.begin();
			chunk.end();
			for(BlockSlot& data : chunk)
			{
				data = block;
			}
		}
		target.checkExits();
		target.regenerateMesh();
	}

	void fillRegionCorners(BlockMapRegion& target, const BlockSlot& block)
	{
		uint32 offset = BlockMapRegion::block_axis_size - 1;
		uvec3 fill_poses[8]
		{
			uvec3(0,		0,		0),
			uvec3(0,		offset,	0),
			uvec3(0,		0,		offset),
			uvec3(0,		offset,	offset),
			uvec3(offset,	0,		0),
			uvec3(offset,	offset,	0),
			uvec3(offset,	0,		offset),
			uvec3(offset,	offset,	offset),
		};

		for(const uvec3& pos : fill_poses)
		{
			uvec3 chunk_index = BlockMapRegion::regionPosToChunkIndex(pos);
			uvec3 chunk_pos = BlockMapRegion::regionPosToChunkPos(pos);
			BlockSlot& target_slot = target[chunk_index][chunk_pos];
			block.data.copyTo(target_slot.data);
		}

		target.checkExits();
		target.regenerateMesh();
	}
}
