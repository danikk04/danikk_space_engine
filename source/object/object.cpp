#include <object/object.h>

namespace danikk_space_engine
{
	thread_local Array<BaseObject*, 16> object_stack;

	void BaseObject::tick()
	{
		object_stack.push(this);
		if(childs != NULL)
		{
			for(index_t i = 0; i < childs->size(); i++)
			{
				BaseObject* child = (*childs)[i];
				child->tick();
			}
		}
		object_stack.pop();
	}

	void BaseObject::frame()
	{
		object_stack.push(this);
		if(childs != NULL)
		{
			for(index_t i = 0; i < childs->size(); i++)
			{
				BaseObject* child = (*childs)[i];
				child->frame();
			}
		}
		object_stack.pop();
	}

	void BaseObject::addChild(BaseObject* obj)
	{
		if (childs == NULL)
		{
			void* ptr = malloc(sizeof(DynamicArray<BaseObject*>));
			childs = new (ptr) DynamicArray<BaseObject*>();
		}
		childs->push(obj);
	}

	danikk_framework::PointerIterable<BaseObject*> BaseObject::iterateChilds()
	{
		danikk_framework::PointerIterable<BaseObject*> result;
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

	bool BaseObject::haveChilds()
	{
		return childs != NULL;
	}
}
