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

		virtual void tick()
		{
			object_stack.push(this);
			for(index_t i = 0; i < childs.size(); i++)
			{
				Object* child = childs[i];
				if(child->exits)
				{
					child->tick();
				}
				else
				{

				}
			}
			object_stack.pop();
		}

		virtual void frame()
		{
			object_stack.push(this);
			for(index_t i = 0; i < childs.size(); i++)
			{
				Object* child = childs[i];
				if(child->exits)
				{
					child->frame();
				}
				else
				{

				}
			}
			object_stack.pop();
		}
	};
}
