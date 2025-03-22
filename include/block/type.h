#pragma once

#include <default.h>

namespace game
{
	class BlockType
	{
	public:
		const char* system_name;
		const char* translated_name;
		Sprite base_texture;

		BlockType(const char* system_name, const Sprite& base_texture);
	};

	extern DynamicArray<BlockType> block_types;

	enum basic_block_types
	{
		air,
		wooden_container,
		light_armor
	};

	void initBasicBlockTypes();
}
