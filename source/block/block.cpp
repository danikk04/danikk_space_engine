#include <default.h>
#include <block/block.h>
#include <danikk_framework/string.h>
#include <danikk_engine/sprite.h>
#include <danikk_engine/texture.h>
#include <localization.h>

namespace game
{
	static String block_formatter = "";

	Block::Block(const char* system_name, Sprite& base_texture)
	{
		this->base_texture = base_texture;
		this->system_name = system_name;

		block_formatter.clear();
		block_formatter << "block/" << system_name;
		translated_name = &localization(block_formatter.c_string());

	};

	DynamicArray<BlockType*> block_types;

	void initBasicBlockTypes()
	{

	}
}
