#include "../internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Sends units to investigate lost hostile contacts during battle reconnaissance. */
	void assignBattleSearchOrders(World& world, UnitGroup& group)
	{
		assignSearchTasks(world, group, UnitSearchTaskType::lost_contact);
	}
}
