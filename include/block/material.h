#pragma once

#include <default.h>
#include <block/block.h>

namespace danikk_space_engine
{
	namespace block
	{
		class Vacuum : public Block
		{
		public:
			static size_t id;

			define_block_constructor(Vacuum, Block){}

			static size_t variablesSize();
		};

		class NotVacuum : public Block
		{
		public:
			vec2 uv_size;
			vec2 uv_offset;
			uint atlas_index;

			static size_t variablesSize();

			define_block_constructor(NotVacuum, Block){}
		};

		class Solid : public NotVacuum
		{
		public:
			static size_t id;

			define_block_constructor(Solid, NotVacuum){}

			static size_t variablesSize();
		};

		class Fluid : public NotVacuum
		{
		public:
			static size_t id;

			define_block_constructor(Fluid, NotVacuum){}

			static size_t variablesSize();
		};

		void InitMaterialTypes();
	}
}
