#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Runs available work assignments for one faction group. */
	void tickWork(World&, UnitGroup&);
	/** @brief Assigns agents to investigate remembered corpse locations. */
	void assignCorpseRecoveryOrders(World&, UnitGroup&);
}
