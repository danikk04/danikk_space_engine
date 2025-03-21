#pragma once

#include <default.h>
#include <object/object.h>

namespace game
{
	class Controller
	{
	public:
		Object* controllable_object = NULL;

		virtual void control() = 0;
	};
}
