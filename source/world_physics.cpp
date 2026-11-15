#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>

namespace danikk_space_engine
{
	void WorldPhysics::tick(World& world)
	{
		Watcher watcher("WorldPhysics::tick");
		integrateMotion(world);
		resolvePlanetCollisions(world);
		resolveBodyCollisions(world);
	}
}
