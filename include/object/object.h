#pragma once

#include <default.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/iterable.h>
#include <block/allocator.h>
#include <danikk_framework/array.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace danikk_space_engine
{
	extern thread_local Array<Object*, 16> object_stack;

	struct ObjectTag
	{
		void tick(){};

		void frame(){};

		void atDispose(){};
	};

	class Object
	{
		DynamicArray<Object*>* childs = NULL;
	public:
		struct TagHeader
		{
			uint32 id;
			uint32 data_offset;

			byte* getTagMemory();

			void tick();

			void frame();

			void atDispose();
		};
		DynamicArray<TagHeader> tags;

		static Object* create(uint32 tag_memory_size);

		template<class tag1_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t));
			uint32 used = sizeof(Object);
			result->pushTag<tag1_t>(used);
			return result;
		}

		template<class tag1_t, class tag2_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t) + sizeof(tag2_t));
			uint32 used = sizeof(Object);
			result->pushTag<tag1_t>(used);
			result->pushTag<tag2_t>(used);
			return result;
		}

		template<class tag1_t, class tag2_t, class tag3_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t) + sizeof(tag2_t) + sizeof(tag3_t));
			uint32 used = sizeof(Object);
			result->pushTag<tag1_t>(used);
			result->pushTag<tag2_t>(used);
			result->pushTag<tag3_t>(used);
			return result;
		}

		template<class tag1_t, class tag2_t, class tag3_t, class tag4_t, class tag5_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t) + sizeof(tag2_t) + sizeof(tag3_t) + sizeof(tag4_t) + sizeof(tag5_t));
			uint32 used = sizeof(Object);
			result->pushTag<tag1_t>(used);
			result->pushTag<tag2_t>(used);
			result->pushTag<tag3_t>(used);
			result->pushTag<tag4_t>(used);
			result->pushTag<tag5_t>(used);
			return result;
		}

		void tick();

		void frame();

		void dispose();

		byte* getTagMemory(uint32 type);

		byte* getTagMemory(index_t i);

		template<class tag_t> void pushTag(uint32& used)
		{
			TagHeader& tag = tags.pushCtor();
			tag.id = tag_t::id;
			tag.data_offset = used;
			byte* tag_data_ptr = (byte*)this + tag.data_offset;
			new ((tag_t*)tag_data_ptr) tag_t();
			used += sizeof(tag_t);
		}

		template<class tag_t> tag_t* getTag()
		{
			return (tag_t*)getTagMemory(tag_t::id);
		}

		template<class tag_t> tag_t* getTag(index_t i)
		{
			return (tag_t*)getTagMemory(i);
		}

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

	template<class tag_t> tag_t* getObjectTag()
	{
		return (tag_t*)getCurrentObject()->getTagMemory(tag_t::id);
	}

	template<class tag_t> tag_t* getParentTag()
	{
		return (tag_t*)getParentObject()->getTagMemory(tag_t::id);
	}

	#define useCurrentObjectTag(type_name, name2) object_tags::type_name* name2 ## _tag = getObjectTag<object_tags::type_name>()

	#define useParentObjectTag(type_name, name2) object_tags::type_name* name2 ## _tag = getParentTag<object_tags::type_name>()

	#define useObjectTag(object, type_name, name2) object_tags::type_name* name2 ## _tag = (object_tags::type_name*)object->getTagMemory(object_tags::type_name::id)
}
