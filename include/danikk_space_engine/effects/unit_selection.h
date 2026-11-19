#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{
	/**
	 * @brief Draws a white ground ring below each currently selected allied unit.
	 * @param world World providing selected component IDs, transforms, and camera position.
	 * Validates each stored slot generation and skips stale or dead humans. A downward
	 * raycast finds the ground beneath each unit; the indicator is hidden when there is
	 * no walkable surface within seven metres of the unit's feet.
	 * The shared annulus mesh follows the surface normal and uses camera-relative translation.
	 */
	void drawUnitSelectionIndicators(World& world);
}
