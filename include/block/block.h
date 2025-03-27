#pragma once

#include <default.h>
#include <danikk_engine/sprite.h>

namespace game
{
	class Block
	{
		public:
		const char* system_name;
		const String* translated_name;
		Sprite base_texture;

		Block() = default;

		Block(const char* system_name, Sprite& base_texture);
	};

	class AirBlock : public Block
	{
		static size_t id;
	public:
		AirBlock() : Block("air", white_sprite){}
	};

	class AbstractContainerBlock : public Block
	{
		static size_t id;
	public:
		AbstractContainerBlock(const char* system_name, const Sprite& base_texture)
			: Block("air", white_sprite){}
	};

	class WoodenContainerBlock : public AbstractContainerBlock
	{
		static size_t id;

		WoodenContainerBlock() : AbstractContainerBlock("air", white_sprite){}
	};

	extern DynamicArray<Block*> block_types;

	void initBasicBlockTypes();
}
