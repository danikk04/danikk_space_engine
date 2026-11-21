#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>
#include <danikk_space_engine/systems/human/data.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Resolves a registered, living unit and its human component. */
	bool getActiveUnit(World&, UnitAgentState&, HumanData*&);
	/** @brief Assigns units to the selected kind of remembered location. */
	void assignSearchTasks(World&, UnitGroup&, UnitSearchTaskType);
	/** @brief Executes stored orders and migrates units after component traversal. */
	void executeUnitOrders(World&, UnitGroup&);
}
