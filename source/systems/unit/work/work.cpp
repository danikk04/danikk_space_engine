#include <danikk_space_engine/profiler.h>

#include "../internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Runs available work assignments, currently post-combat corpse recovery. */
	void tickWork(World& world, UnitGroup& group)
	{
		DSE_WATCH();
		assignCorpseRecoveryOrders(world, group);
	}
}
