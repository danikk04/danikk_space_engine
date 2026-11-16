#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the orientation and projection settings of a point-attached camera.
	 */
	struct CameraComponent
	{
		vec3 surface_up = vec3(0.0f, 0.0f, 1.0f);
		vec3 yaw_reference_forward = vec3(1.0f, 0.0f, 0.0f);
		vec3 rotation = vec3(0.0f);
		float vertical_fov_degrees = 90.0f;
		float near_clip_distance = 0.1f;
		float far_clip_distance = 10000.0f;

		/**
		 * @brief Builds the camera forward vector from its surface-up basis, yaw reference, and pitch.
		 */
		vec3 getFront() const;

		/**
		 * @brief Builds the camera up vector from forward and roll, preserving the surface-oriented camera basis.
		 */
		vec3 getUp() const;

		/**
		 * @brief Returns the normalized right axis perpendicular to the camera forward and up vectors.
		 */
		vec3 getRight() const;
	};

	/**
	 * @brief Constructs camera component.
	 */
	void constructCameraComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves camera component.
	 */
	void moveCameraComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases camera component.
	 */
	void destroyCameraComponent(void* data);

	/**
	 * @brief Runs one simulation tick for camera component.
	 */
	void tickCameraComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Processes one frame for camera component.
	 */
	void frameCameraComponent(PointComponentSlot& slot, World& world);
}
