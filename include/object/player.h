#pragma once

#include <default.h>
#include <object/object.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class PlayerController : public Object
		{
		public:
			static uint32 id;

			void tick();
		};
	}
}
