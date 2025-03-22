#pragma once

#include <block/type.h>
#include <danikk_framework/string.h>

namespace game
{
	static String block_formatter = "";

	BlockType::BlockType(const char* system_name, const Sprite& base_texture)
	{
		this->base_texture = base_texture;
		this->system_name = system_name;

		block_formatter.clear();
		block_formatter << "block/" << system_name;
		translated_name = localization(block_formatter);

	};

	class AirBlockType : public BlockType
	{
	public:
		AirBlockType() : BlockType("air", white_texture){}
	};

	class ContainerBlockType : public BlockType
	{
	public:

	};

	class WoodenContainerBlockType : public ContainerBlockType
	{
		WoodenContainerBlockType() : BlockType("air", white_texture){}
	};

	DynamicArray<BlockType> block_types;

	void initBasicBlockTypes()
	{

	}
}
