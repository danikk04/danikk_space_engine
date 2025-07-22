#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void addFrameCube(BlockMapObject& target, uvec3 pos);//куб, у которого есть только грани

	void fillRegion(BlockMapObject& target, uvec3 pos, uint id);
}
