#include <preset/block/inserter.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void BlockInserter::insertBlock(uvec3 pos, const BlockData& block)
	{
		//logInfo(pos);
		target.getBlock(pos + offset) = block;
	}
}
