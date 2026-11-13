#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	struct MachinegunComponent
	{
		float scale = 0.25f;
		vec3 rotation = vec3(0.0f);
		float shot_timer = 0.0f;
	};

	void constructMachinegunComponent(void* destination, const void* init_data);
	void moveMachinegunComponent(void* destination, void* source);
	void destroyMachinegunComponent(void* data);
	void tickMachinegunComponent(PointComponentSlot& slot, World& world);
	void frameMachinegunComponent(PointComponentSlot& slot, World& world);
}
