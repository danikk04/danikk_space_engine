#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine
{
	void PlanetSystem::destroyPlanetTerrainTree(PlanetData& planet)
	{
		DSE_WATCH();
		planet_terrain_tree_internal::destroyTree(planet);
	}

	void PlanetSystem::updatePlanetTerrainTree(PlanetData& planet,
		const glm::dvec3& camera_position, double height)
	{
		DSE_WATCH();
		planet_terrain_tree_internal::UpdateContext context{
			planet, planet.terrain_tree, camera_position, height,
			getPlanetSettings(planet.settings_id), world.telemetry};

		planet_terrain_tree_internal::initialize(context);
		planet_terrain_tree_internal::calculateDetailBudget(context);
		planet_terrain_tree_internal::refine(context);
		planet_terrain_tree_internal::collectMeshJobCandidates(context);
		planet_terrain_tree_internal::activateMeshes(context);
		planet_terrain_tree_internal::recordDiagnostics(context);
		planet_terrain_tree_internal::evictCachedMeshes(context);
		appendPlanetJobCandidates(planet);
	}
}
