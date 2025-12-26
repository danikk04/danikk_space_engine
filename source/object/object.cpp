#include <object/object.h>
#include <object/tag_table.h>

namespace danikk_space_engine
{
	thread_local Array<Object*, 16> object_stack;

	Object* Object::create(uint32 tag_memory_size)
	{
		Object* result;
		result = (Object*)malloc(sizeof(Object) + tag_memory_size);
		new (result) Object();
		return result;
	}

	void Object::tick()
	{
		object_stack.push(this);
		for(index_t i = 0; i < tags.size(); i++)
		{
			TagHeader* tag = &tags[i];
			tag->tick();
		}
		if(childs != NULL)
		{
			for(index_t i = 0; i < childs->size(); i++)
			{
				if(!(((*childs)[i])->tags.size() > 0))
				{
					free((*childs)[i]);
					(*childs)[i] = childs->pop();
				}
			}
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
		for(index_t j = 0; j < tags.size(); j++)
		{
			TagHeader* tag = &tags[j];
			tag->frame();
		}
		object_stack.pop();
	}

	void Object::dispose()
	{
		index_t i = tags.size() - 1;
		while(i --> 0)
		{
			tags[i].atDispose();
		}
		tags.clear();
	}

	byte* Object::getTagMemory(uint32 type)
	{
		for(TagHeader& tag : tags)
		{
			if(tag.id == type)
			{
				return (byte*)this + tag.data_offset;
			}
		}
		return NULL;
	}

	byte* Object::getTagMemory(index_t i)
	{
		return (byte*)this + tags[i].data_offset;
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

	byte* Object::TagHeader::getTagMemory()
	{
		byte* data = (byte*)object_stack.peek();
		data += data_offset;
		return data;
	}

	void Object::TagHeader::tick()
	{
		Tag* tag = danikk_space_engine::getTag(id);
		tag->tick((ObjectTag*)getTagMemory());
	}

	void Object::TagHeader::frame()
	{
		danikk_space_engine::getTag(id)->frame((ObjectTag*)getTagMemory());
	}

	void Object::TagHeader::atDispose()
	{
		danikk_space_engine::getTag(id)->atDispose((ObjectTag*)getTagMemory());
	}
}
