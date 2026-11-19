#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/**
	 * @brief Draws movement arrows and destination markers for selected allied units.
	 * @param world World providing selection, assigned NPC orders, and camera transforms.
	 * Units without a valid active move order have no indicator. Geometry is batched
	 * into camera-facing ribbons and markers in camera-relative coordinates.
	 */
	void drawUnitOrderIndicators(World& world);
}
