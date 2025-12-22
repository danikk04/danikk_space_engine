#pragma once

#include <default.h>
#include <controller/controller.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class PlayerController : public Controller
		{
		public:
			static uint32 id;

			void tick();

			void virtual frame(){};

			bool virtual atDispose(){return 0;};
		};
	}
}
