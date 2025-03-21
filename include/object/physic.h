#pragma once

#include <object/world.h>

namespace game
{
	class PhysicObject : virtual public WorldObject
	{
	public:
		vec3 speed;
		vec3 rotation_speed;
		float mass;

		void tick() override
		{
			pos += speed;
			rotation += rotation_speed;
		}
	};
}
