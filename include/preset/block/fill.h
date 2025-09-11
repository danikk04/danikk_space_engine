#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void fillRegion(const BlockSlot& block);

	void fillRegionCorners(const BlockSlot& block);

	void fillRegionCenters(const BlockSlot& block, int radius = 1);

	void fillRegionLine(const BlockSlot& block, const pos_type& start, const pos_type& end);

	void fillRandomRegionLine(const BlockSlot& block, bool can_diagonal = true);

	void fillYRegionLevel(const BlockSlot& block, int y);
}
