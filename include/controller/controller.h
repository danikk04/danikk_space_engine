#pragma once

#include <default.h>
#include <object/object.h>

namespace danikk_space_engine
{
	class Controller
	{
	public:
		Object* controllable_object = NULL;

		virtual void control() = 0;
	};
}
