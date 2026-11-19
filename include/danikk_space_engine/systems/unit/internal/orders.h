#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Resolves a registered, living unit and its human component. */
	bool getActiveUnit(World&, UnitAgentState&, PointComponentSlot*&, HumanComponent*&);
	/** @brief Assigns units to the selected kind of remembered location. */
	void assignSearchTasks(World&, UnitGroup&, UnitSearchTaskType);
	/** @brief Executes stored orders and migrates units after component traversal. */
	void executeUnitOrders(World&, UnitGroup&);
	/** @brief Moves a unit and its attached component slots to its new loaded chunk. */
	void updateUnitChunk(World&, PointComponentSlot&);
}
