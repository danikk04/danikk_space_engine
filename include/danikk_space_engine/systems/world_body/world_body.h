#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/systems/faction/faction.h>

namespace danikk_space_engine
{
	struct WorldBodySpatialEntry
	{
		PointId point_id = 0;
		glm::dvec3 position = glm::dvec3(0.0);
	};

	struct WorldBodySpatialNode
	{
		glm::dvec3 center = glm::dvec3(0.0);
		double half_extent = 0.0;
		uint32 children[8] = {~uint32(0), ~uint32(0), ~uint32(0), ~uint32(0),
			~uint32(0), ~uint32(0), ~uint32(0), ~uint32(0)};
		DynamicArray<WorldBodySpatialEntry> entries;
		bool hasChildren() const
		{
			for(uint32 child_id : children) if(child_id != ~uint32(0)) return true;
			return false;
		}
	};

	/** @brief Body-local adaptive octree for point-owned entities. */
	struct WorldBodySpatialTree
	{
		static constexpr double root_half_extent = 8192000.0;
		static constexpr double minimum_half_extent = 4.0;
		static constexpr size_t split_threshold = 32;
		DynamicArray<WorldBodySpatialNode> nodes;
		DynamicArray<uint32> free_node_ids;
		uint32 root_id = ~uint32(0);

		/** @brief Inserts a point not already indexed by this tree. */
		void insert(PointId point_id, const glm::dvec3& position);
		void remove(PointId point_id, const glm::dvec3& position);
		void collectRayCandidates(const glm::dvec3& origin, const glm::dvec3& direction,
			double max_distance, DynamicArray<PointId>& output) const;
		void collectBoxCandidates(const glm::dvec3& center, double half_extent,
			DynamicArray<PointId>& output) const;
	};

	/**
	 * @brief Represents a positioned world body and indexes its points.
	 *
	 * `point_ids` contains pooled resource IDs managed by PointData. Body
	 * destruction coordinates point, planet, and human lifetimes. Position,
	 * orientation, and velocity are expressed in world coordinates; the point
	 * spatial tree uses body-local coordinates.
	 */
	class WorldBody
	{
	public:
		/** @brief Planet whose spatial frame this body proxies, or invalid PlanetId. */
		PlanetId planet_id = static_cast<PlanetId>(-1);
		/** @brief Faction owning this body, or no_faction if unassigned. */
		FactionId faction_id = no_faction;
		/** @brief Body origin in double-precision world coordinates. */
		glm::dvec3 position = glm::dvec3(0.0);
		/** @brief Body orientation mapping body-local coordinates into world orientation. */
		glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
		/** @brief Linear velocity in world units per second, integrated by PhysicsSystem. */
		glm::dvec3 linear_velocity = glm::dvec3(0.0);
		/** @brief Angular velocity vector in radians per second, integrated by PhysicsSystem. */
		glm::dvec3 angular_velocity = glm::dvec3(0.0);
		/** @brief Body mass used by gravity and collision response. */
		double mass = 1.0;
		/** @brief Conservative radius used by body-level broadphase collision checks. */
		double collision_radius = 0.0;
		/** @brief Point IDs stored in this body's local coordinate frame. */
		DynamicArray<PointId> point_ids;
		/** @brief Adaptive spatial index for points in this body's local coordinate frame. */
		WorldBodySpatialTree spatial_tree;
	};

}
