#pragma once

#include <object/object.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class LimitedLive : public ObjectTag
		{
		public:
			static uint32 id;

			uint32 tick_to_live = 0;

			bool16 ticked = false;
			bool16 in_map_check = false;

			void tick()
			{
				if(ticked)
				{
					if(tick_to_live == 0)
					{
						getCurrentObject()->dispose();
					}
					else
					{
						tick_to_live--;
					}
				}
				if(in_map_check)
				{

				}
			}
		};
	}
}
