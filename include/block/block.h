#pragma once

#include <default.h>

namespace game
{
	namespace block
	{
		class Block
		{
			public:
			const char* system_name;
			const String* translated_name;

			Block() = default;

			Block(const char* system_name);

			virtual void tick();

			static size_t variablesSize();
		};

		#define define_block_constructor(TYPE, PARENT) TYPE(const char* system_name) : PARENT(system_name)

		#define add_block_type(TYPE, SYSTEM_NAME) TYPE::id = generateId(); block_table.push(new TYPE(SYSTEM_NAME));//createSprite

		void initBlockTypes();

		uint generateId();
	}

	extern DynamicArray<block::Block*> block_table;
}
