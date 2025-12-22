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

	struct ObjectTag
	{
		void virtual tick() = 0;

		void virtual frame() = 0;

		bool virtual atDispose() = 0;
	};

	class Object
	{
		bool32 exits = true;
		uint32 used = sizeof(Object);
	public:
		DynamicArray<Object*> childs;
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
			result->pushTag<tag1_t>();
			return result;
		}

		template<class tag1_t, class tag2_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t) + sizeof(tag2_t));
			result->pushTag<tag1_t>();
			result->pushTag<tag2_t>();
			return result;
		}

		template<class tag1_t, class tag2_t, class tag3_t> static Object* create()
		{
			Object* result = create(sizeof(tag1_t) + sizeof(tag2_t) + sizeof(tag3_t));
			result->pushTag<tag1_t>();
			result->pushTag<tag2_t>();
			result->pushTag<tag3_t>();
			return result;
		}

		void tick();

		void frame();

		void dispose();

		byte* getTagMemory(uint32 type);

		template<class tag_t> void pushTag()
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
	};

	inline Object* getObject()
	{
		return object_stack.peek();
	}

	template<class tag_t> tag_t* getObjectTag(uint32 type)
	{
		return (tag_t*)object_stack.peek()->getTagMemory(tag_t::id);
	}

	#define useObjectTag(name, name2) object_tags::name* name2 ## tag = getObjectTag(object_tags::name::id)
}
