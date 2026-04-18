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

	inline Object* getCurrentObject()
	{
		return object_stack.peek();
	}

	inline Object* getParentObject()
	{
		return object_stack[object_stack.size() - 2];
	}

	#define useCurrentObjectTag(type_name, name2) object_tags::type_name* name2 ## _tag = getObjectTag<object_tags::type_name>()

	#define useParentObjectTag(type_name, name2) object_tags::type_name* name2 ## _tag = getParentTag<object_tags::type_name>()

	#define useObjectTag(object, type_name, name2) object_tags::type_name* name2 ## _tag = (object_tags::type_name*)object->getTagMemory(object_tags::type_name::id)
}
