#pragma once

#include <glm/glm.hpp>

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/**
	 * @brief Owns the active camera's point binding, orientation, movement, and projection.
	 *
	 * The camera is world-level state rather than a point component: its point remains
	 * chunk-owned for spatial positioning, while this system handles camera input and view data.
	 */
	class CameraSystem
	{
		/** @brief World providing input, point storage, and coordinate transforms. */
		World& world;
		/** @brief Point used as the camera origin, or invalid when no camera is bound. */
		PointId point_id = static_cast<PointId>(-1);
		/** @brief Chunk owning the camera point, or invalid when no camera is bound. */
		ChunkId chunk_id = static_cast<ChunkId>(-1);

	public:
		/** @brief Clears the camera binding and logs subsystem initialization. */
		void init();
		/** @brief Clears the camera binding and logs subsystem shutdown. */
		void shutdown();
		/** @brief Local radial-up direction used to keep the camera upright relative to its body. */
		vec3 surface_up = vec3(0.0f, 0.0f, 1.0f);
		/** @brief Tangent-plane heading used as the zero-yaw reference. */
		vec3 yaw_reference_forward = vec3(1.0f, 0.0f, 0.0f);
		/** @brief Yaw, pitch, and roll angles in radians. */
		vec3 rotation = vec3(0.0f);
		/** @brief Vertical perspective field of view. */
		float vertical_fov_degrees = 90.0f;
		/** @brief Near clipping plane distance in world units. */
		float near_clip_distance = 0.1f;
		/** @brief Far clipping plane distance in world units. */
		float far_clip_distance = 10000.0f;

		/** @brief Binds camera state to a chunk-owned point. */
		void bind(PointId point_id, ChunkId chunk_id);
		/** @brief Clears the camera binding. */
		void unbind();
		/** @brief Returns whether the bound point and owning chunk are still active. */
		bool isActive() const;
		/** @brief Returns the bound point ID, or the invalid ID when unbound. */
		PointId getPointId() const { return point_id; }
		/** @brief Returns the owning chunk ID, or the invalid ID when unbound. */
		ChunkId getChunkId() const { return chunk_id; }
		/** @brief Returns camera forward in the camera point's local frame. */
		vec3 getFront() const;
		/** @brief Returns camera up in the camera point's local frame. */
		vec3 getUp() const;
		/** @brief Returns camera right in the camera point's local frame. */
		vec3 getRight() const;
		/** @brief Returns the camera point position in double-precision world coordinates. */
		glm::dvec3 getWorldPosition() const;
		/** @brief Processes camera input and moves the bound point for one simulation tick. */
		void tick();
		/** @brief Validates projection settings and publishes the active perspective matrix. */
		void frame() const;

		/** @brief Binds the camera system to its owning World. */
		explicit CameraSystem(World& world) : world(world) {}
	};
}
