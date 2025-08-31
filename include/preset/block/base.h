#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void fillRegion(BlockMapRegion& target, const BlockSlot& block);

	void fillRegionCorners(BlockMapRegion& target, const BlockSlot& block);

	void fillRegionLine(BlockMapRegion& target, const BlockSlot& block, const pos_type& start, const pos_type& end);

	void fillRandomRegionLine(BlockMapRegion& target, const BlockSlot& block, bool can_diagonal = true);

	void fillYRegionLevel(lockMapRegion& target, const BlockSlot& block)
}
