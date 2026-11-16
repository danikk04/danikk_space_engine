#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>

namespace danikk_space_engine
{

	/**
	 * @brief Performs the tick operation.
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
