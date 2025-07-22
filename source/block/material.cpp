#include <default.h>
#include <danikk_framework/string.h>
#include <danikk_engine/sprite.h>
#include <localization.h>

#include <block/block.h>
#include <block/material.h>

namespace danikk_space_engine
{
	namespace block
	{
		size_t Vacuum::id;
		size_t Solid::id;
		size_t Fluid::id;

		void InitMaterialTypes()
		{
			add_block_type(Vacuum, "vacuum");
			add_block_type(Solid, "solid");
			add_block_type(Fluid, "fluid");
		}
	}
}
