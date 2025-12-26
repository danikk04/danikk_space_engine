#pragma once

#include <object/world.h>

#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class Kinetic : public ObjectTag
		{
		public:
			static uint32 id;

			vec3 speed;
			vec3 rotation_speed;
			float mass = 0;
			float temperature = 0;

			void tick()
			{
				object_tags::World* world = object_stack.peek()->getTag<object_tags::World>();
				world->pos += speed * getTargetFrameDelay();
				world->rotation += rotation_speed * getTargetFrameDelay();
			}
		};
	}
}
