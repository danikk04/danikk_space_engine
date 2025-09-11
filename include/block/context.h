#pragma once

//#include <object/block_map.h>
//#include <block/data.h>
#include <default.h>
#include <danikk_framework/array.h>

namespace danikk_space_engine
{
	struct BlockContext
	{
		BlockSlot* block = NULL;
		BlockMapChunk* chunk = NULL;
		BlockMapRegion* region = NULL;
		BlockMapObject* map = NULL;

		pos_type chunk_pos;
		pos_type block_pos;

		pos_type getGlobalBlockOffset();
	};

	extern thread_local Array<BlockContext*, 8> current_context_stack;

	struct BlockContextUser
	{
		BlockContextUser(BlockContext* context)
		{
			current_context_stack.push(context);
		}

		~BlockContextUser()
		{
			current_context_stack.pop();
		}
	};

#define current_block_context current_context_stack.last()
}
