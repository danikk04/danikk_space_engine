#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	struct MachinegunComponent
	{
		float scale = 0.25f;
		vec3 forward = vec3(1.0f, 0.0f, 0.0f);
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		vec3 right = vec3(0.0f, 0.0f, 1.0f);
		float shot_timer = 0.0f;
	};

	void constructMachinegunComponent(void* destination, const void* init_data);
	void moveMachinegunComponent(void* destination, void* source);
	void destroyMachinegunComponent(void* data);
	void tickMachinegunComponent(PointComponentSlot& slot, World& world);
	void frameMachinegunComponent(PointComponentSlot& slot, World& world);
}
