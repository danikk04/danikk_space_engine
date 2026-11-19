#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Leaves agents without an assigned job in the idle rest state. */
	void tickRest(UnitGroup&);
}
