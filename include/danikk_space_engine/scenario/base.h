#pragma once

#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine
{

	/**
	 * @brief Resets world data pools and creates the default Earth planet, camera, and local controller.
	 *
	 * The scenario only assembles initial resources; planet octree mesh generation remains asynchronous
	 * and is requested later by PlanetSystem during its frame update. The camera stores
	 * a body-local position on the planet body.
	 * @param world World receiving the scenario resources and component slots.
	 * The camera is attached to the Earth planet body. Its
	 * position is above the surface by default; when configured for ground spawn,
	 * the scenario searches for a sufficiently thick solid region and falls back
	 * to the sky position if none is found.
	 */
	void startBaseScenario(World& world);
}
