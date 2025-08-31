#pragma once

#include <default.h>
#include <block/allocator.h>

namespace danikk_space_engine
{
	struct BlockBaseHeader
	{
		uint32 id = 0;


		uint32 main_material_id = 0;

		float main_material_mass = 0;
		float temperature = 0;

		uint32 child_blocks_count;

		Block* getBlockType();
	};

	struct BlockSlot
	{
		RegionMemoryBlock data;

		bool isHeaderExits()
		{
			return data.size() >= sizeof(BlockBaseHeader);
		}

		BlockBaseHeader& getHeader()
		{
			assert(data.size() > 0);
			return *(BlockBaseHeader*)data.ptr();
		}

		BlockBaseHeader& createHeader()
		{
			assert(data.size() == 0);
			data.resize(sizeof(BlockBaseHeader));
			return *(BlockBaseHeader*)data.ptr();
		}

		BlockBaseHeader& getCreateHeader()
		{
			if(data.size() < sizeof(BlockBaseHeader))
			{
				data.resize(sizeof(BlockBaseHeader));
			}
			return *(BlockBaseHeader*)data.ptr();
		}

		uint32 getId()
		{
			if (data.size() == 0)
			{
				return 0;
			}
			else
			{
				return (*(BlockBaseHeader*)data.ptr()).id;
			}
		}
	};

	struct BlockDataNode
	{

	};

	extern thread_local BlockSlot* current_block;
}
