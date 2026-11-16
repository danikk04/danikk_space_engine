#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the orientation and projection settings of a point-attached camera.
	 */
	struct CameraComponent
	{
		/** @brief Local radial-up direction used to keep the camera upright relative to its world body. */
		vec3 surface_up = vec3(0.0f, 0.0f, 1.0f);
		/** @brief Tangent-plane heading used as the zero-yaw reference. */
		vec3 yaw_reference_forward = vec3(1.0f, 0.0f, 0.0f);
		/** @brief Yaw, pitch, and roll in radians in x/y/z; yaw and pitch define view before roll is applied. */
		vec3 rotation = vec3(0.0f);
		/** @brief Vertical perspective field of view used to build the projection matrix. */
		float vertical_fov_degrees = 90.0f;
		/** @brief Near clipping plane distance in world units. */
		float near_clip_distance = 0.1f;
		/** @brief Far clipping plane distance in world units. */
		float far_clip_distance = 10000.0f;

		/**
		 * @brief Computes the normalized viewing direction from the surface frame, yaw, and pitch.
		 * @return Camera forward axis in the camera point's local coordinate system.
		 * Positive pitch looks toward `surface_up`; yaw rotates the tangent heading
		 * around that up axis.
		 */
		vec3 getFront() const;

		/**
		 * @brief Computes the camera up axis after rolling its surface-oriented basis around the view direction.
		 * @return Normalized camera up axis in local coordinates.
		 */
		vec3 getUp() const;

		/**
		 * @brief Computes the camera right axis as the cross product of forward and rolled up.
		 * @return Normalized camera right axis in local coordinates.
		 */
		vec3 getRight() const;
	};

	/**
	 * @brief Placement-constructs a camera component, copying optional initial state.
	 * @param destination Address of storage supplied by the component slot.
	 * @param init_data Optional pointer to a `CameraComponent`; null selects defaults.
	 */
	void constructCameraComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs a camera component during slot relocation.
	 * @param destination Address of the new payload storage.
	 * @param source Address of the existing live payload, destroyed after moving.
	 */
	void moveCameraComponent(void* destination, void* source);

	/**
	 * @brief Runs the camera component destructor without freeing slot-owned storage.
	 * @param data Address of the live camera payload.
	 */
	void destroyCameraComponent(void* data);

	/**
	 * @brief Applies mouse look, Q/E roll, and frame-scaled WASD movement to the camera point.
	 *
	 * The update transports the yaw frame when the camera's radial up direction changes and
	 * rebases large local offsets into the owning chunk to preserve float precision. Mouse motion
	 * is rotated by the current roll so horizontal and vertical input remain aligned with the window.
	 * During Body -> Drag with left mouse and Alt held, camera mouse-look is suppressed so the
	 * controller can use the cursor delta to rotate the captured body.
	 * @param slot Slot containing this camera and its point/chunk IDs.
	 * @param world World providing input context, transforms, and component state.
	 */
	void tickCameraComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Validates the camera clip/FOV values and publishes a perspective projection matrix.
	 * @param slot Slot containing the camera payload and attached point ID.
	 * @param world World whose active camera projection is being configured.
	 */
	void frameCameraComponent(PointComponentSlot& slot, World& world);
}
