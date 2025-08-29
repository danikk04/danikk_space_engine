#pragma once

#include <default.h>
#include <object/block_map.h>
#include <block/data.h>

namespace danikk_space_engine
{
	void (*block_iter_func_t)(BlockSlot*, BlockSlot*);

	void pairIterateChunk(BlockMapObject& region, block_iter_func_t block_iter_func)
	{

	}

	void pairIterateRegion(BlockMapObject& region, block_iter_func_t block_iter_func)
	{

	}

	void pairIterateMap(BlockMapObject& map, block_iter_func_t block_iter_func)
	{

	}
}
