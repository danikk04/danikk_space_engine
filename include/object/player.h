#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>

namespace danikk_space_engine
{
	class Player : public WorldObject
	{
	public:
		static uint32 id;

		void tick();
	};
}
