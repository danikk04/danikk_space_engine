#include <preset/block/base.h>
#include <preset/block/inserter.h>
#include <block/container.h>
#include <danikk_framework/glm.h>

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
	}

	void fillRegionCorners(BlockMapRegion& target, const BlockSlot& block)
	{
		current_region = &target;
		uint32 offset = BlockMapRegion::block_axis_size - 1;
		pos_type fill_poses[8]
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

		for(const pos_type& pos : fill_poses)
		{
			pos_type chunk_index = BlockMapRegion::regionPosToChunkIndex(pos);
			pos_type chunk_pos = BlockMapRegion::regionPosToChunkPos(pos);
			BlockSlot& target_slot = target[chunk_index][chunk_pos];
			block.data.copyTo(target_slot.data);
		}
		current_region = NULL;
	}

	void fillRegionLine(BlockMapRegion& target, const BlockSlot& block, const pos_type& start, const pos_type& end)
	{
		current_region = &target;
		vec3 vector_delta = vec3(end) - vec3(start);
		vec3 delta = glm::normalize(vector_delta);
		vec3 current_pos = start;

		for(size_t iter_count = size_t(glm::length(vector_delta)); iter_count > 0; iter_count--)
		{
			pos_type pos = current_pos;
			pos_type chunk_index = BlockMapRegion::regionPosToChunkIndex(pos);
			pos_type chunk_pos = BlockMapRegion::regionPosToChunkPos(pos);
			BlockSlot& target_slot = target[chunk_index][chunk_pos];
			assert((void*)target.begin() <= (void*)&target_slot);
			assert((void*)target.end() >= (void*)&target_slot);
			block.data.copyTo(target_slot.data);

			current_pos += delta;
		}
		current_region = NULL;
	}

	void fillRandomRegionLine(BlockMapRegion& target, const BlockSlot& block, bool can_diagonal)
	{
		pos_type start = target.randPos();
		pos_type end = target.randPos();
		if(!can_diagonal)
		{
			int32 rand_index = start.x % 3;
			end[rand_index] = start[rand_index];
		}
		fillRegionLine(target, block, start, end);
	}
}
