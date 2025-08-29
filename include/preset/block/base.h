#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void fillRegion(BlockMapRegion& target, const BlockSlot& block);

	void fillRegionCorners(BlockMapRegion& target, const BlockSlot& block);
}
