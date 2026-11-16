#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores machine gun state, fires bullets, and draws its model.
	 */
	struct MachinegunComponent
	{
		float scale = 0.25f;
		vec3 forward = vec3(1.0f, 0.0f, 0.0f);
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		vec3 right = vec3(0.0f, 0.0f, 1.0f);
		float shot_timer = 0.0f;
	};

	/**
	 * @brief Constructs machinegun component.
	 */
	void constructMachinegunComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves machinegun component.
	 */
	void moveMachinegunComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases machinegun component.
	 */
	void destroyMachinegunComponent(void* data);

	/**
	 * @brief Runs one simulation tick for machinegun component.
	 */
	void tickMachinegunComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Processes one frame for machinegun component.
	 */
	void frameMachinegunComponent(PointComponentSlot& slot, World& world);
}
