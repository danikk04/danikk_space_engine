#pragma once

#include <danikk_engine/danikk_engine.h>
#include "posed.h"

namespace danikk_space_engine
{
	class KineticObject : public PosedObject
	{
	public:
		vec3 speed;
		vec3 rotation_speed;
		float mass = 0;
		float temperature = 0;

		void tick()
		{
			pos += speed * getTargetFrameDelay();
			rotation += rotation_speed * getTargetFrameDelay();
		}
	};
}
