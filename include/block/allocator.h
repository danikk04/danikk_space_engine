#pragma once

#include <default.h>
#include <danikk_framework/memory.h>

namespace danikk_space_engine
{
	class MonolithAllocator;

	MonolithAllocator& getCurrentAllocator();

	class MonolithMemoryBlock
	{
	private:
		uint32 offset = 0;
		uint32 m_size = 0;

		friend class MonolithAllocator;
	public:
		MonolithMemoryBlock() = default;

		void resize(uint32 new_size, MonolithAllocator& allocator = getCurrentAllocator());

		void free(MonolithAllocator& allocator = getCurrentAllocator());

		void nullify(MonolithAllocator& allocator = getCurrentAllocator());

		byte* ptr(MonolithAllocator& allocator = getCurrentAllocator());

		const byte* ptr(MonolithAllocator& allocator = getCurrentAllocator()) const;

		void copyTo(MonolithMemoryBlock& other, MonolithAllocator& allocator = getCurrentAllocator()) const;

		uint32 size();
	};

	class MonolithAllocator//Все данные хранит в одном большом куске
	{
	private:
		byte* data;

		uint32 used = 0;
		uint32 capacity = 0;

		DynamicArray<MonolithMemoryBlock> free_blocks;

		friend class MonolithMemoryBlock;
	public:
		void reserve(uint32 size);

		MonolithMemoryBlock popNewBlock(uint32 size);

		void resize(MonolithMemoryBlock& block, uint32 new_size);
	};
}
