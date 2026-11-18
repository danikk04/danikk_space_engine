#pragma once

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{

	/**
	 * @brief Submits the planet's pending immutable generation requests to ChunkLoader.
	 * @param planet Planet state containing requests produced by updatePlanetChunks().
	 * @param world World whose worker queue receives those requests.
	 * @param slot Component identity copied into each request for stale-result checks.
	 */
	void queuePlanetRequests(PlanetComponent& planet, World& world, const WorldBodyComponentSlot& slot);

	/**
	 * @brief Unloads chunks outside the desired set while retaining chunks containing unexpired corpses.
	 * @param planet Lookup describing desired coordinates and currently loaded chunks.
	 * @param world WorldData that owns their resources and human component state.
	 */
	void unloadPlanetChunksOutsideDesiredSet(PlanetComponent& planet, World& world);

	/**
	 * @brief Computes the camera-centered chunk set and schedules generation for missing records.
	 * @param planet Planet state whose lookup and update position are refreshed.
	 * @param slot Planet component identity attached to the owning WorldBody.
	 * @param world World used for unloads, selection invalidation, and worker requests.
	 * @param camera_position Camera coordinate relative to the planet center.
	 */
	void updatePlanetChunks(
		PlanetComponent& planet,
		WorldBodyComponentSlot& slot,
		World& world,
		const glm::dvec3& camera_position);

	/**
	 * @brief Invalidates generated chunks intersecting an edited spherical region.
	 * @param planet Planet lookup containing loaded chunk records.
	 * @param world WorldData owning the chunk resources.
	 * @param center Edit center relative to the planet origin.
	 * @param radius Edit stamp radius in world units.
	 */
	void invalidatePlanetChunksAroundEdit(
		PlanetComponent& planet,
		World& world,
		const glm::dvec3& center,
		double radius);
}
