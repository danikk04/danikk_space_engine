#include <preset/block/base.h>
#include <preset/block/inserter.h>
#include <block/container.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void fillRegion(const BlockSlot& block)
	{
		for(BlockMapChunk& chunk : getCurrentRegion())
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
			BlockSlot& target_slot = getCurrentRegion()[chunk_index][chunk_pos];
			target_slot = block;
		}
	}

	void fillRegionLine(const BlockSlot& block, const pos_type& start, const pos_type& end)
	{
		pos_type vector_delta = end - start;
		vec3 delta = glm::normalize(vec3(vector_delta));
		vec3 current_pos = start;

		for(size_t iter_count = size_t(glm::length(vec3(vector_delta))) + 2; iter_count > 0; iter_count--)
		{
			pos_type pos = current_pos;
			pos_type chunk_index = BlockMapRegion::regionPosToChunkIndex(pos);
			pos_type chunk_pos = BlockMapRegion::regionPosToChunkPos(pos);
			BlockSlot& target_slot = getCurrentRegion()[chunk_index][chunk_pos];
			assert((void*)getCurrentRegion().begin() <= (void*)&target_slot);
			assert((void*)getCurrentRegion().end() >= (void*)&target_slot);
			target_slot = block;

			current_pos += delta;
		}
	}

	void fillRandomRegionLine(const BlockSlot& block, bool can_diagonal)
	{
		pos_type start = getCurrentRegion().randPos();
		pos_type end = getCurrentRegion().randPos();
		if(!can_diagonal)
		{
			int32 rand_index = start.x % 3;
			end[rand_index] = start[rand_index];
		}
		fillRegionLine(block, start, end);
	}

	void fillYRegionLevel(const BlockSlot& block, int y)
	{
		fatalError("not implemented");
	}
}
