#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the motion and lifetime of a projectile.
	 */
	struct BulletComponent
	{
		vec3 velocity = vec3(0.0f);
		float age = 0.0f;
		float lifetime = 5.0f;
		bool destruction_queued = false;
	};

	/**
	 * @brief Constructs bullet component.
	 */
	void constructBulletComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves bullet component.
	 */
	void moveBulletComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases bullet component.
	 */
	void destroyBulletComponent(void* data);

	/**
	 * @brief Runs one simulation tick for bullet component.
	 */
	void tickBulletComponent(PointComponentSlot& slot, World& world);

	/**
	 * @brief Processes one frame for bullet component.
	 */
	void frameBulletComponent(PointComponentSlot& slot, World& world);
}
