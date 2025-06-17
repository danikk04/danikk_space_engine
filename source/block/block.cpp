#include <default.h>
#include <danikk_framework/string.h>
#include <danikk_engine/sprite.h>
#include <danikk_engine/texture.h>
#include <localization.h>

#include <block/block.h>
#include <block/container.h>
#include <block/material.h>

#include <asset.h>

namespace game
{
	DynamicArray<block::Block*> block_table;
	namespace block
	{
		static String block_formatter = "";
		uint last_id = 0;


		Block::Block(const char* system_name)
		{
			this->system_name = system_name;

			block_formatter.clear();
			block_formatter << "block/" << system_name;
			translated_name = &localization(block_formatter.c_string());

		};

		size_t Block::variablesSize()
		{
			return 0;
		}

		void Block::tick()
		{

		}

		uint generateId()
		{
			return last_id++;
		}

		void initBlockTypes()
		{
			InitMaterialTypes();
			add_block_type(WoodenContainer, "wooden_container");
		}
	}
}
