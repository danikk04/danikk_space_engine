#pragma once

#include <default.h>
#include <controller/controller.h>

namespace danikk_space_engine
{
	class PlayerController : public Controller
	{
	public:
		 void control() final;
	};
}
