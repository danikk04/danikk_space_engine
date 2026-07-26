#pragma once

#include <object/kinetic.h>

namespace danikk_space_engine
{
	class WorldObject : public PosedObject
	{
		float temp;
		uint32 self_index;
	public:
		void tick()
		{

		}

		float getApproxRadius()
		{
			return 1.0f;
		}
	};
}
