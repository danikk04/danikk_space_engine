#pragma once

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{

	/**
	 * @brief Queues planet requests.
	 */
	void queuePlanetRequests(PlanetComponent& planet, World& world, const WorldBodyComponentSlot& slot);

	/**
	 * @brief Unloads planet chunks outside desired set.
	 */
	void unloadPlanetChunksOutsideDesiredSet(PlanetComponent& planet, World& world);

	/**
	 * @brief Clears planet face selection.
	 */
	void clearPlanetFaceSelection(World& world);

	/**
	 * @brief Updates planet chunks.
	 */
	void updatePlanetChunks(
		PlanetComponent& planet,
		WorldBodyComponentSlot& slot,
		World& world,
		const glm::dvec3& camera_position);

	/**
	 * @brief Invalidates planet chunks around edit.
	 */
	void invalidatePlanetChunksAroundEdit(
		PlanetComponent& planet,
		World& world,
		const glm::dvec3& center,
		double radius);
}
