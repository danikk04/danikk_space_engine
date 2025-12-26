#pragma once

#include <default.h>
#include <block/allocator.h>
#include <object/object.h>

namespace danikk_space_engine
{
	struct BlockSlot : public Object
	{
		uint32 id = 0;


		Block* getBlockType();
	};
}
