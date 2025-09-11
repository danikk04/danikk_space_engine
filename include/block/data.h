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

		struct flags_t
		{
			bool16 is_must_copy;//по умолчанию может генерировать карту, где будет куча блоков
		};

		uint16 child_blocks_count;
		flags_t flags;

		Block* getBlockType();
	};

	struct BlockSlot
	{
		MonolithMemoryBlock data;

		bool isHeaderExits()
		{
			if(data.size() >= kib(8))
			{
				logError("find too huge block!!!");
				return false;
			}
			///assert(data.size() < kib(8));
			return data.size() >= sizeof(BlockBaseHeader);
		}

		BlockBaseHeader& getHeader()
		{
			assert(data.size() > 0);
			BlockBaseHeader* header = (BlockBaseHeader*)data.ptr();
			if(header->flags.is_must_copy)
			{
				size_t prev_size = data.size();
				data.nullify();
				data.resize(prev_size);
				BlockBaseHeader* new_header = (BlockBaseHeader*)data.ptr();
				memcpy((void*)new_header, (void*)header, prev_size);
				return *new_header;
			}
			return *header;
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
			assert(data.size() < kib(8));
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
