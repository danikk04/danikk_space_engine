#pragma once

#include <danikk_space_engine/component/point/point.h>

namespace danikk_space_engine
{
	struct BulletComponent
	{
		vec3 velocity = vec3(0.0f);
		float age = 0.0f;
		float lifetime = 5.0f;
		bool destruction_queued = false;
	};

	void constructBulletComponent(void* destination, const void* init_data);
	void moveBulletComponent(void* destination, void* source);
	void destroyBulletComponent(void* data);
	void tickBulletComponent(PointComponentSlot& slot, World& world);
	void frameBulletComponent(PointComponentSlot& slot, World& world);
}
