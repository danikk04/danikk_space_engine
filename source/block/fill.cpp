#include <block/fill.h>
#include <danikk_framework/glm.h>
#include <block/context.h>
#include <block/pos.h>

namespace danikk_space_engine
{
	void fillRegion(const BlockSlot& block)
	{
		for(BlockMapChunk& chunk : *current_block_context->region)
		{
			chunk.begin();
			chunk.end();
			for(BlockSlot& target_slot : chunk)
			{
				target_slot = block;
			}
		}
	}

	void fillRegionCorners(const BlockSlot& block)
	{
		uint32 offset = BlockMapRegion::block_axis_size - 1;
		ivec3 fill_poses[8]
		{
			ivec3(0,		0,		0),
			ivec3(0,		offset,	0),
			ivec3(0,		0,		offset),
			ivec3(0,		offset,	offset),
			ivec3(offset,	0,		0),
			ivec3(offset,	offset,	0),
			ivec3(offset,	0,		offset),
			ivec3(offset,	offset,	offset),
		};
		for(const ivec3& pos : fill_poses)
		{
			global_pos_type& context_pos = current_block_context->pos;
			context_pos.setChunkBlockPos(pos);
			BlockSlot& target_slot = (*current_block_context->region)[context_pos.getChunkPos()][context_pos.getBlockPos()];
			target_slot = block;
		}
	}
}
