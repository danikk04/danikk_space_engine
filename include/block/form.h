#pragma once

#include <default.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	inline bool isInCilinder(uvec3 in_structure_pos, uvec3 structure_size)
	{
	    if(in_structure_pos.y >= structure_size.y)
	    {
	        return false;
	    }
	    int center_x = structure_size.x / 2;
	    int center_z = structure_size.z / 2;

	    int relative_x = in_structure_pos.x - center_x;
	    int relative_z = in_structure_pos.z - center_z;

	    int cilynder_hypotenuse = center_x * center_x + center_z * center_z;
	    int relative_hypotenuse = relative_x * relative_x + relative_z * relative_z;

	    return relative_hypotenuse <= cilynder_hypotenuse;
	}

	inline void generatePlantHole(uvec3 in_structure_pos, uvec3 structure_size, BlockSlot& target)
	{

	}
}
