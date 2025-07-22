#include <preset/block/base.h>
#include <preset/block/inserter.h>
#include <block/container.h>

namespace danikk_space_engine
{
	void addFrameCube(BlockMapObject& target, uvec3 pos, uint id)
	{
		constexpr static size_t cube_size = 16;

		BlockInserter inserter = BlockInserter(target, pos);

		BlockData block;
		block.id = id;

		for(uint iterable_axis = 0; iterable_axis < 3; iterable_axis++)
		{
			uint not_iterable_axis[2];

			switch(iterable_axis)
			{
				case 0: not_iterable_axis[0] = 1; not_iterable_axis[1] = 2; break;
				case 1: not_iterable_axis[0] = 0; not_iterable_axis[1] = 2; break;
				case 2: not_iterable_axis[0] = 1; not_iterable_axis[1] = 0; break;
			}

			for(uint iterable_axis_pos = 0; iterable_axis_pos < cube_size; iterable_axis_pos++)
			{
				uvec3 block_place_pos;
				block_place_pos[iterable_axis] = iterable_axis_pos;

				for(uint place_side1 = 0; place_side1 < 2; place_side1++)
				{
					for(uint place_side2 = 0; place_side2 < 2; place_side2++)
					{
						block_place_pos[not_iterable_axis[0]] = place_side1 ? 0 : cube_size - 1;
						block_place_pos[not_iterable_axis[1]] = place_side2 ? 0 : cube_size - 1;
						inserter.insertBlock(block_place_pos, block);
					}
				}
			}
		}
		target.checkExits();
		target.regenerateMesh();
	}

	void fillRegion(BlockMapObject& target, uvec3 pos, uint id)
	{
		uvec3 region_index = BlockMapObject::globalPosToRegionIndex(pos);
		BlockMapRegion& region = target[region_index];

		for(BlockMapChunk& chunk : region)
		{
			chunk.begin();
			chunk.end();
			for(BlockData& data : chunk)
			{
				data.id = id;
			}
		}
		target.checkExits();
		target.regenerateMesh();
	}
}
