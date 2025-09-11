#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <block/allocator.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	extern thread_local Array<Object*, 16> object_stack;

	class Object
	{
		bool32 exits = true;
	public:
		DynamicArray<Object*> childs;

		virtual void tick();

		virtual void frame();

		virtual void atDispose();

		void dispose();
	};

	template<class type> type* createObject()
	{
		return new Object();
	}

	template<class type> void* voidCreateObject()
	{
		return new Object();
	}
}
