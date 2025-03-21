#pragma once

#include <default.h>
#include <controller/controller.h>

namespace game
{
	class PlayerController : public Controller
	{
	public:
		 void control() final;
	};
}
