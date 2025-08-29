#include <preset/block/inserter.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void BlockInserter::insertBlock(pos_type pos, const BlockSlot& block)
	{
		//logInfo(pos);
		target.getBlock(pos + offset) = block;
	}
}
