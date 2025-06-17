#include <preset/block/inserter.h>
#include <danikk_framework/glm.h>

namespace game
{
	void BlockInserter::insertBlock(uvec3 pos, const BlockData& block)
	{
		//logInfo(pos);
		target.getBlock(pos + offset) = block;
	}
}
