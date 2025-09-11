#include <block/context.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	thread_local Array<BlockContext*, 8> current_context_stack;

	pos_type BlockContext::getGlobalBlockOffset()
	{
		return region->pos * (int)BlockMapRegion::block_axis_size + chunk_pos * (int)BlockMapChunk::axis_size;
	}
}
