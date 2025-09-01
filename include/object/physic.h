#pragma once

#include <object/world.h>

#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	class PhysicObject : public virtual WorldObject
	{
	public:
		vec3 speed;
		vec3 rotation_speed;
		float mass = 0;
		float temperature = 0;

		void tick() override
		{
			WorldObject::tick();
			pos += speed * getTargetFrameDelay();
			rotation += rotation_speed * getTargetFrameDelay();
		}
	};
}
