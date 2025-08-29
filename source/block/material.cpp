#include <default.h>
#include <danikk_framework/string.h>
#include <danikk_engine/sprite.h>
#include <localization.h>

#include <block/block.h>
#include <block/material.h>

namespace danikk_space_engine
{
	size_t Vacuum::id;
	size_t Solid::id;
	size_t Fluid::id;
	size_t SolidRaw::id;

	void InitMaterialTypes()
	{
		add_block_type(Vacuum, "vacuum");
		add_block_type(Solid, "solid");
		add_block_type(Fluid, "fluid");
		add_block_type(SolidRaw, "solid raw");
	}
}
