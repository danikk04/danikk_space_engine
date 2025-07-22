#pragma once

#include <default.h>
#include <danikk_engine/sprite.h>

namespace danikk_space_engine
{
	namespace block
	{
		struct BlockTag
		{
			char key[8];
			char value[8];

			void* getValuePtr()
			{
				return (void*)&value[0];
			}

			template<class value_t> value_t& getValue()
			{
				return (value_t&)*getValuePtr();
			}
		};

		template<size_t tag_count> class BlockTagContainer
		{
			BlockTag data[tag_count];
		};
	}
}
