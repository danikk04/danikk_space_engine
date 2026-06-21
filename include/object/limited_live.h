#pragma once

#include <object/object.h>

namespace danikk_space_engine
{
	class LimitedLiveObject : public Object
	{
	public:
		static uint32 id;

		uint32 tick_to_live = 0;
		uint32 ticked = false;

		void tick()
		{
			if(ticked)
			{
				if(tick_to_live == 0)
				{

				}
				else
				{
					tick_to_live--;
				}
			}
		}
	};
}
