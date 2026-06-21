#include <object/object.h>

namespace danikk_space_engine
{
	thread_local Array<Object*, 16> object_stack;

	void Object::tick()
	{
		object_stack.push(this);
		if(childs != NULL)
		{
			for(index_t i = 0; i < childs->size(); i++)
			{
				Object* child = (*childs)[i];
				child->tick();
			}
		}
		object_stack.pop();
	}

	void Object::frame()
	{
		object_stack.push(this);
		if(childs != NULL)
		{
			for(index_t i = 0; i < childs->size(); i++)
			{
				Object* child = (*childs)[i];
				child->frame();
			}
		}
		object_stack.pop();
	}

	void Object::addChild(Object* obj)
	{
		if (childs == NULL)
		{
			void* ptr = malloc(sizeof(DynamicArray<Object*>));
			childs = new (ptr) DynamicArray<Object*>();
		}
		childs->push(obj);
	}

	danikk_framework::PointerIterable<Object*> Object::iterateChilds()
	{
		danikk_framework::PointerIterable<Object*> result;
		if(childs == NULL)
		{
			result._begin = NULL;
			result._end = NULL;
		}
		else
		{
			result._begin = childs->begin();
			result._end = childs->end();
		}
		return result;
	}

	bool Object::haveChilds()
	{
		return childs != NULL;
	}
}
