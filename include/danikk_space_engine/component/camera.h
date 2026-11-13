#pragma once

#include <danikk_space_engine/component/point_component.h>

namespace danikk_space_engine
{
	struct CameraComponent
	{
		vec3 rotation = vec3(0.0f);
		float vertical_fov_degrees = 90.0f;
		float near_clip_distance = 0.1f;
		float far_clip_distance = 10000.0f;

		vec3 getFront() const;
		vec3 getUp() const;
		vec3 getRight() const;
	};

	void constructCameraComponent(void* destination, const void* init_data);
	void moveCameraComponent(void* destination, void* source);
	void destroyCameraComponent(void* data);
	void tickCameraComponent(PointComponentSlot& slot, World& world);
	void frameCameraComponent(PointComponentSlot& slot, World& world);
}
