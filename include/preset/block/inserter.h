#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	class BlockInserter
	{
		BlockMapObject& target;
		pos_type offset;
	public:
		BlockInserter(BlockMapObject& target, pos_type offset) : target(target), offset(offset){}

		void insertBlock(pos_type pos, const BlockSlot& block);
	};
}
