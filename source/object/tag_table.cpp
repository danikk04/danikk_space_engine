#include <object/tag_table.h>

namespace danikk_space_engine
{
	namespace internal
	{
		DynamicArray<Tag> tag_table;
		uint32 tag_id = 1;

		uint32 generateTagId()
		{
			return tag_id++;
		}
	}
}
