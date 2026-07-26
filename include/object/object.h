#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/iterable.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	extern thread_local Array<BaseObject*, 16> object_stack;

	class BaseObject
	{
		struct child_data
		{
			DynamicArray<BaseObject*> objects;
			DynamicArray<byte> data;
		};
		child_data* childs;
	public:

		void tick();

		void frame();

		void addChild(BaseObject* obj);

		template<class object_t, class... args_t>  createObject(const args_t&... args)
		{

		}

		template<class object_t> static object_t* create()
		{
			object_t* result;
			result = (object_t*)malloc(sizeof(object_t));
			new (result) object_t();
			return result;
		}

		danikk_framework::PointerIterable<BaseObject*> iterateChilds();

		bool haveChilds();
	};

	inline BaseObject* getParentObject()
	{
		return object_stack[object_stack.size() - 2];
	}
}
