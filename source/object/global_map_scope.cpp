#include <block/context.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	thread_local Array<BlockContext*, 8> current_context_stack;
}
