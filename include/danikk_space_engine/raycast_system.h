#pragma once

#include <cstddef>

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/**
	 * @brief Stores the nearest terrain or BuildPart hit returned by a ray query.
	 *
	 * A miss leaves `hit` false. The face's owning body and chunk IDs identify its
	 * current geometry; a BuildPart hit also supplies its component slot ID.
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
		/** @brief True when the nearest hit is a BuildPart component. */
		bool hit_build_part = false;
		/** @brief True when the hit was found in a planet scalar field. */
		bool hit_terrain = false;
		/** @brief WorldBody containing the hit geometry. */
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		/** @brief Chunk containing the hit geometry or point component. */
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		/** @brief BuildPart component slot, valid when `hit_build_part` is true. */
		PointComponentSlotId build_part_slot_id = static_cast<PointComponentSlotId>(-1);
	};

	/** @brief Performs ray queries against planet terrain and BuildPart geometry. */
	class RaycastSystem
	{
		World& world;
		RaycastHit castBodies(const WorldBodyId* body_ids, size_t body_count,
			const glm::dvec3& origin, const vec3& direction, float max_distance) const;

	public:
		/** @brief Binds ray queries to the owning World. */
		explicit RaycastSystem(World& world) : world(world) {}

		/** @brief Casts against all active WorldBodies and returns the nearest hit. */
		RaycastHit cast(const glm::dvec3& origin, const vec3& direction,
			float max_distance = 10000.0f) const;
		/** @brief Casts against one WorldBody. */
		RaycastHit castBody(WorldBodyId body_id, const glm::dvec3& origin,
			const vec3& direction, float max_distance) const;
		/** @brief Casts from the active camera along its forward axis. */
		RaycastHit castFromCamera(float max_distance = 10000.0f) const;
	};
}
