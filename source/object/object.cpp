#include <object/object.h>

namespace danikk_space_engine
{
	thread_local Array<Object*, 16> object_stack;

	void Object::tick()
	{
		object_stack.push(this);
		for(index_t i = 0; i < childs.size(); i++)
		{
			if(!childs[i]->exits)
			{
				childs[i] = childs.pop();
			}
		}
		for(index_t i = 0; i < childs.size(); i++)
		{
			Object* child = childs[i];
			child->tick();
		}
		object_stack.pop();
	}

	void Object::frame()
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
}
