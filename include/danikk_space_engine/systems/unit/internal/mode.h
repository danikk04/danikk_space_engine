#pragma once

#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Applies group-wide battle entry and exit rules from current observations. */
	void updateGroupMode(UnitGroup&, float dt);
}
