#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/resource_pool.h>
#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	/** @brief Owns WorldBody instances and their point, transform, and lifecycle operations. */
	class WorldBodySystem
	{
		/** @brief World whose point, planet, and human state is coordinated during body removal. */
		World& world;
		/** @brief Stable-ID pool for positioned WorldBody resources. */
		WorldResourcePool<WorldBody, WorldBodyId> world_bodies{"WorldBody"};

	public:
		/** @brief Binds body lifecycle operations to their owning World. */
		explicit WorldBodySystem(World& world) : world(world) {}
		/** @brief Creates a default WorldBody and returns its ID. */
		WorldBodyId createWorldBody();
		/** @brief Attaches a point to a body's local frame and spatial tree. */
		void attachPointToBody(WorldBodyId body_id, PointId point_id,
			const glm::dvec3& body_local_position);
		/** @brief Converts a body's local point position to world coordinates. */
		glm::dvec3 getPointWorldPosition(PointId point_id) const;
		/** @brief Builds a camera-relative world transform for a point. */
		mat4 getPointWorldMatrix(PointId point_id) const;
		/** @brief Moves a point in its body frame and updates the spatial tree. */
		void setPointBodyLocalPosition(PointId point_id, const glm::dvec3& body_local_position);
		/** @brief Destroys a body's humans, points, planet, mesh, and resource. */
		void destroyWorldBody(WorldBodyId id);
		/** @brief Checks whether a WorldBody ID is active. */
		bool isWorldBodyActive(WorldBodyId id) const;
		/** @brief Returns active WorldBody IDs; invalidated by pool mutations. */
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;
		/** @brief Gets a checked active WorldBody. */
		WorldBody& getWorldBody(WorldBodyId id);
		/** @brief Gets a checked active WorldBody without mutation access. */
		const WorldBody& getWorldBody(WorldBodyId id) const;
		/** @brief Returns allocated WorldBody slots, including reusable slots. */
		size_t getWorldBodySlotCount() const;
		/** @brief Destroys active body resources while retaining capacity. */
		void clear();
		/** @brief Destroys active body resources and releases capacity. */
		void release();
	};
}
