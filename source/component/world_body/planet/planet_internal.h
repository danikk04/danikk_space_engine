#pragma once

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	void queuePlanetRequests(PlanetComponent& planet, World& world, const WorldBodyComponentSlot& slot);
	void unloadPlanetChunksOutsideDesiredSet(PlanetComponent& planet, World& world);
	void clearPlanetFaceSelection(World& world);
	void updatePlanetChunks(
		PlanetComponent& planet,
		WorldBodyComponentSlot& slot,
		World& world,
		const glm::dvec3& camera_position);
	void invalidatePlanetChunksAroundEdit(
		PlanetComponent& planet,
		World& world,
		const glm::dvec3& center,
		double radius);
}
