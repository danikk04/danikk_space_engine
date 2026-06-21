#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/iterable.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	extern thread_local Array<Object*, 16> object_stack;

	class Object
	{
		DynamicArray<Object*>* childs = NULL;
	public:

		void tick();

		void frame();

		void addChild(Object* obj);

		danikk_framework::PointerIterable<Object*> iterateChilds();

		bool haveChilds();
	};

	inline Object* getParentObject()
	{
		return object_stack[object_stack.size() - 2];
	}
}
