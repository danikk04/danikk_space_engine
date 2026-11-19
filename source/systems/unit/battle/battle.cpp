#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/unit/internal/battle.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Runs target selection, contact investigation, and battle reconnaissance. */
	void tickBattle(World& world, UnitGroup& group)
	{
		DSE_WATCH();
		assignOrders(world, group);
		assignBattleSearchOrders(world, group);
		assignBattleReconnaissanceOrders(world, group);
	}
}
