#pragma once

#include <object/object.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class Timed : public ObjectTag
		{
		public:
			static uint32 id;

			size_t tick_to_live = 0;

			void tick()
			{
				if(tick_to_live == 0)
				{
					dispose();
				}
				else
				{
					tick_to_live--;
				}
			}
		};
	}
}
