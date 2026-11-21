#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/** @brief Sends adaptive terrain-node mesh requests to the shared worker. */
	void PlanetSystem::queuePlanetRequests(PlanetData& planet)
	{
		DSE_WATCH();
		for(MeshGenerationRequest& request : planet.pending_requests)
		{
			request.world_body_id = planet.world_body_id;
			request.planet_generation_id = planet.generation_id;
		}
		world.mesh_generation.replacePlanetRequests(
			planet.world_body_id, planet.generation_id, planet.pending_requests);
	}
}
