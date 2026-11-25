#pragma once

#include <glm/glm.hpp>

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Identifies which camera projection range is active for the current render pass. */
	enum class CameraProjectionMode : uint8
	{
		near_clip,
		far_clip
	};

	/**
	 * @brief Owns the active camera's body-local position, orientation, and projection.
	 *
	 * The camera is world-level state attached directly to a WorldBody through a body-local position.
	 */
	class CameraSystem
	{
		/** @brief World providing input, body storage, and coordinate transforms. */
		World& world;
		/** @brief WorldBody carrying the camera, or invalid when no camera is bound. */
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		/** @brief Camera position in the owning body's local coordinate frame. */
		glm::dvec3 body_local_position = glm::dvec3(0.0);
		/** @brief Near and far planes selected for the next projection upload. */
		float projection_near_clip_distance = 0.1f;
		float projection_far_clip_distance = 10000.0f;
		/** @brief Projection pass label associated with the currently selected clip planes. */
		CameraProjectionMode projection_mode = CameraProjectionMode::near_clip;

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

		/** @brief Binds camera state to a position in a WorldBody's local frame. */
		void bind(WorldBodyId body_id, const glm::dvec3& body_local_position);
		/** @brief Clears the camera binding. */
		void unbind();
		/**
		 * @brief Selects a named projection pass and its clipping planes for subsequent applyProjection() calls.
		 * @param mode Labels the near or far render pass.
		 * @param near_clip_distance Distance to the pass's near clipping plane.
		 * @param far_clip_distance Distance to the pass's far clipping plane.
		 */
		void setProjectionMode(CameraProjectionMode mode, float near_clip_distance,
			float far_clip_distance);
		/** @brief Returns whether the bound WorldBody is still active. */
		bool isActive() const;
		/** @brief Returns the bound WorldBody ID, or the invalid ID when unbound. */
		WorldBodyId getWorldBodyId() const { return body_id; }
		/** @brief Returns the camera's position in its WorldBody's local frame. */
		const glm::dvec3& getBodyLocalPosition() const { return body_local_position; }
		/** @brief Applies a displacement in the owning body's local coordinate frame. */
		void moveBodyLocal(const glm::dvec3& displacement);
		/** @brief Returns camera forward in the body-local frame. */
		vec3 getFront() const;
		/** @brief Returns camera up in the body-local frame. */
		vec3 getUp() const;
		/** @brief Returns camera right in the body-local frame. */
		vec3 getRight() const;
		/** @brief Returns the camera position in double-precision world coordinates. */
		glm::dvec3 getWorldPosition() const;
		/** @brief Validates projection settings and uploads the active perspective matrix to the current shader. */
		void applyProjection() const;

		/** @brief Binds the camera system to its owning World. */
		explicit CameraSystem(World& world) : world(world) {}
	};
}
