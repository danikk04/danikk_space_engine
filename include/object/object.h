#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	extern thread_local Array<Object*, 16> object_stack;

	class Object
	{
	public:
		DynamicArray<Object*> childs;

		bool32 exits = true;

		virtual void tick();

		virtual void frame();
	};
}
