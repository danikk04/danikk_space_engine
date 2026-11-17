#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>

namespace danikk_space_engine
{

	/**
	 * @brief Executes the physics stages in their fixed per-tick order.
	 * @param world World passed through to each stage.
	 * Runs integration, core removal, planet contacts, body-to-body contacts,
	 * and human separation. Since core removal follows integration, bodies and
	 * point entities that cross the exclusion radius are released before contact
	 * resolution in this tick.
	 */
	void WorldPhysics::tick(World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		integrateMotion(world);
		destroyEntitiesInsidePlanetCore(world);
		resolvePlanetCollisions(world);
		resolveBodyCollisions(world);
		resolveHumanCollisions(world);
	}
}
