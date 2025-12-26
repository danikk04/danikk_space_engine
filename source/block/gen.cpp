#include <danikk_framework/glm.h>

#include <default.h>

#include <block/gen.h>
#include <block/fill.h>
#include <block/context.h>
#include <block/pos.h>

namespace danikk_space_engine
{
	void generateMap()
	{

		/*for(BlockMapChunk& chunk : *current_block_context->region)
		{

		}*/
	}


	/*void cutCilinder(global_pos_type pos, ivec2 size)
	{
        for(int y = -size.y; y <= size.y; y++)
        {
    	    for(int x = -size.x; x <= size.x; x++)
    	    {
    	    	int x2 = x * x;
                for(int z = -size.x; z <= size.x; z++)
                {
                	int z2 = z * z;
                	if (x2 + z2 <= size.x)
                	{
                		current_block_map
                	}
                }
    	    }
        }
	}*/

	void generatePlantHole(uvec3 in_structure_pos, uvec3 structure_size, BlockSlot& target)
	{

	}
}
