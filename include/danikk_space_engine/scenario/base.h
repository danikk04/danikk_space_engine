#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Resets world data pools and creates the default Earth planet, camera, and local controller.
	 *
	 * The scenario only assembles initial resources; planet chunk generation remains asynchronous
	 * and is requested later by the PlanetComponent during its frame callback. The camera is stored
	 * as a point in a dedicated chunk owned by the planet body, with the controller attached to
	 * that same point.
	 * @param world World receiving the scenario resources and component slots.
	 * The camera point is placed in a chunk owned by the Earth planet body. Its
	 * position is above the surface by default; when configured for ground spawn,
	 * the scenario searches for a sufficiently thick solid region and falls back
	 * to the sky position if none is found.
	 */
	void startBaseScenario(World& world);
}
