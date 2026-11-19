#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Chooses and assigns battle behavior for one faction group. */
	void tickBattle(World&, UnitGroup&);
	/** @brief Assigns attack and pursuit orders from the group's current intelligence. */
	void assignOrders(World&, UnitGroup&);
	/** @brief Assigns investigation orders for hostile contacts lost during battle. */
	void assignBattleSearchOrders(World&, UnitGroup&);
	/** @brief Disperses units and scans surroundings during battle reconnaissance. */
	void assignBattleReconnaissanceOrders(World&, UnitGroup&);
}
