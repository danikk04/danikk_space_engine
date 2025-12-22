#pragma once

#include <object/object.h>

namespace danikk_space_engine
{
	struct Tag
	{
		uint32 id;

		void (*tick)(ObjectTag*);

		void (*frame)(ObjectTag*);

		bool (*atDispose)(ObjectTag*);
	};

	namespace internal
	{
		extern DynamicArray<Tag> tag_table;

		uint32 generateTagId();
	}

	template<class tag_t> void addTag()
	{
		Tag& new_tag = internal::tag_table.pushCtor();
		tag_t::id = internal::generateTagId();
		new_tag.id = tag_t::id;
		new_tag.tick = (void (*)(ObjectTag*))&tag_t::tick;
		new_tag.frame = (void (*)(ObjectTag*))&tag_t::frame;
		new_tag.atDispose = (bool (*)(ObjectTag*))&tag_t::atDispose;
	}

	inline Tag* getTag(uint32 id)
	{
		for(Tag& tag : internal::tag_table)
		{
			if (tag.id == id)
			{
				return &tag;
			}
		}
		return NULL;
	}

	void generateTagsId();
}
