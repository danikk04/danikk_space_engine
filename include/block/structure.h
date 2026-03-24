#pragma once

#include <object/world.h>

#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		typedef void (*structure_gen_function_t)(uvec3 in_structure_pos, uvec3 structure_size, BlockSlot& target);

		struct GeneratingStructure
		{
			structure_gen_function_t structure_gen_function;
			uvec3 size;
		};

		class StructureGen : ObjectTag
		{
		public:
			static uint32 id;

			uvec3 in_structure_pos;
		};
	}
}

