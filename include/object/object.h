#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace game
{
	extern thread_local Array<Object*, 16> object_stack;

	class Object
	{
	public:
		DynamicArray<Object*> childs;

		virtual void tick()
		{
			object_stack.push(this);
			for(Object* el : childs)
			{
				el->tick();
			}
			object_stack.pop();
		}

		virtual void frame()
		{
			object_stack.push(this);
			for(Object* el : childs)
			{
				el->frame();
			}
			object_stack.pop();
		}
	};
}
