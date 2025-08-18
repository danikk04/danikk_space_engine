#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void addFrameCube(BlockMapRegion& target, uint id);//куб, у которого есть только грани

	void fillRegion(BlockMapRegion& target, uint id);

	void fillRegionCorners(BlockMapRegion& target, uint id);
}
