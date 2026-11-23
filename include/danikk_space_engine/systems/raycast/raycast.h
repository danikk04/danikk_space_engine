#pragma once

#include <cstddef>

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/systems/planet/data.h>

namespace danikk_space_engine
{
	/**
	 * @brief Stores the nearest terrain or BuildPart hit returned by a ray query.
	 *
	 * A miss leaves `hit` false. The owning body identifies the geometry; a
	 * BuildPart hit also supplies its BuildPart ID.
	 */
	struct RaycastHit
	{
		/** @brief True when the ray intersects supported geometry. */
		bool hit = false;
		/** @brief Distance from the ray origin to the nearest hit. */
		float distance = 0.0f;
		/** @brief Nearest hit position in world coordinates. */
		glm::dvec3 position = glm::dvec3(0.0);
		/** @brief Surface normal transformed into world orientation. */
		vec3 normal = vec3(0.0f);
		/** @brief True when the nearest hit is a BuildPart instance. */
		bool hit_build_part = false;
		/** @brief True when the hit was found in a planet scalar field. */
		bool hit_terrain = false;
		/** @brief WorldBody containing the hit geometry. */
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		/** @brief Planet containing a terrain hit, even when it has no WorldBody proxy. */
		PlanetId planet_id = static_cast<PlanetId>(-1);
		/** @brief BuildPart ID, valid when `hit_build_part` is true. */
		BuildPartId build_part_id = static_cast<BuildPartId>(-1);
	};

	/** @brief Performs ray queries against planet terrain and BuildPart geometry. */
	class RaycastSystem
	{
		World& world;
		RaycastHit castBodies(const WorldBodyId* body_ids, size_t body_count,
			const glm::dvec3& origin, const vec3& direction, float max_distance) const;
		/** @brief Finds a ray crossing of a planet's scalar-density surface. */
		bool raycastPlanetScalarField(PlanetId planet_id,
			const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
			PlanetTerrainMode mode,
			double& distance, glm::dvec3& normal) const;

	public:
		/** @brief Binds ray queries to the owning World. */
		explicit RaycastSystem(World& world) : world(world) {}
		/** @brief Logs initialization of the raycast subsystem. */
		void init();
		/** @brief Logs shutdown of the raycast subsystem. */
		void shutdown();

		/** @brief Casts against all planets and active WorldBodies and returns the nearest hit. */
		RaycastHit cast(const glm::dvec3& origin, const vec3& direction,
			float max_distance = 10000.0f) const;
		/** @brief Casts against one WorldBody. */
		RaycastHit castBody(WorldBodyId body_id, const glm::dvec3& origin,
			const vec3& direction, float max_distance) const;
		/** @brief Casts from the active camera along its forward axis. */
		RaycastHit castFromCamera(float max_distance = 10000.0f) const;
	};
}
