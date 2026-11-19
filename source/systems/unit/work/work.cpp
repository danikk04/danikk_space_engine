#include <danikk_space_engine/profiler.h>

#include <danikk_space_engine/systems/unit/internal/state.h>
#include <danikk_space_engine/systems/unit/internal/work.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Runs available work assignments, currently post-combat corpse recovery. */
	void tickWork(World& world, UnitGroup& group)
	{
		DSE_WATCH();
		assignCorpseRecoveryOrders(world, group);
	}
}
