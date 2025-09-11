#pragma once

#include <object/object.h>

namespace danikk_space_engine
{
	class TimedObject : public virtual Object
	{
	public:
		size_t tick_to_live = 0;

		void tick() override
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
