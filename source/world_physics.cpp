#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>

namespace danikk_space_engine
{

	/**
	 * @brief Integrates body motion, resolves planet contacts, resolves body-to-body contacts, and removes core-crossing entities.
	 */
	void WorldPhysics::tick(World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("WorldPhysics::tick");
		integrateMotion(world);
		destroyEntitiesInsidePlanetCore(world);
		resolvePlanetCollisions(world);
		resolveBodyCollisions(world);
		resolveHumanCollisions(world);
	}
}
