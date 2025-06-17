#pragma once

#include <default.h>
#include <object/block_map.h>

namespace game
{
	class BlockInserter
	{
		BlockMapObject& target;
		uvec3 offset;
	public:
		BlockInserter(BlockMapObject& target, uvec3 offset) : target(target), offset(offset){}

		void insertBlock(uvec3 pos, const BlockData& block);
	};
}
