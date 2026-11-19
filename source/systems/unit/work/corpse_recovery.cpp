#include "../internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Assigns agents to inspect the remembered locations of corpses. */
	void assignCorpseRecoveryOrders(World& world, UnitGroup& group)
	{
		assignSearchTasks(world, group, UnitSearchTaskType::corpse_recovery);
	}
}
