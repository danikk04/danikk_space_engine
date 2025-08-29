#pragma once

#include <default.h>
#include <block/block.h>

namespace danikk_space_engine
{
	class Vacuum : public Block
	{
	public:
		static size_t id;

		define_block_constructor(Vacuum, Block){}
	};

	class NotVacuum : public Block
	{
	public:
		vec2 uv_size;
		vec2 uv_offset;
		uint atlas_index;

		define_block_constructor(NotVacuum, Block){}
	};

	class Solid : public NotVacuum
	{
	public:
		static size_t id;

		define_block_constructor(Solid, NotVacuum){}
	};

	class SolidRaw : public Solid
	{
	public:
		static size_t id;

		define_block_constructor(SolidRaw, Solid){}
	};

	class Fluid : public NotVacuum
	{
	public:
		static size_t id;

		define_block_constructor(Fluid, NotVacuum){}
	};

	void InitMaterialTypes();
}
