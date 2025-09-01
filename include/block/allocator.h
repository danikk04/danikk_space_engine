#pragma once

#include <default.h>
#include <danikk_framework/memory.h>

namespace danikk_space_engine
{
	class RegionAllocator;

	RegionAllocator& getCurrentAllocator();

	class RegionMemoryBlock
	{
	private:
		uint32 offset = 0;
		uint32 m_size = 0;

		friend class RegionAllocator;
	public:
		RegionMemoryBlock() = default;

		void resize(uint32 new_size, RegionAllocator& allocator = getCurrentAllocator());

		void free(RegionAllocator& allocator = getCurrentAllocator());

		void nullify(RegionAllocator& allocator = getCurrentAllocator());

		byte* ptr(RegionAllocator& allocator = getCurrentAllocator());

		const byte* ptr(RegionAllocator& allocator = getCurrentAllocator()) const;

		void copyTo(RegionMemoryBlock& other, RegionAllocator& allocator = getCurrentAllocator()) const;

		uint32 size();
	};

	class RegionAllocator//Все данные хранит в одном большом куске, все данные выделяются только для одного региона
	{
	private:
		byte* data;

		uint32 used = 0;
		uint32 capacity = 0;

		DynamicArray<RegionMemoryBlock> free_blocks;

		friend class RegionMemoryBlock;
	public:
		void reserve(uint32 size);

		RegionMemoryBlock popNewBlock(uint32 size);

		void resize(RegionMemoryBlock& block, uint32 new_size);
	};

}
