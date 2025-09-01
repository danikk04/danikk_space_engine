#include <block/allocator.h>
#include <object/block_map.h>

namespace danikk_space_engine
{
	void RegionMemoryBlock::resize(uint32 new_size, RegionAllocator& allocator)
	{
		allocator.resize(*this, new_size);
	}

	void RegionMemoryBlock::free(RegionAllocator& allocator)
	{
		if(m_size != 0)
		{
			allocator.free_blocks.push(*this);
			offset = m_size = 0;
		}
	}

	void RegionMemoryBlock::nullify(RegionAllocator& allocator)
	{
		offset = m_size = 0;
	}

	byte* RegionMemoryBlock::ptr(RegionAllocator& allocator)
	{
		return allocator.data + offset;
	}

	const byte* RegionMemoryBlock::ptr(RegionAllocator& allocator) const
	{
		return allocator.data + offset;
	}

	void RegionMemoryBlock::copyTo(RegionMemoryBlock& other, RegionAllocator& allocator) const
	{
		other.resize(this->m_size, allocator);
		memcpy(other.ptr(allocator), ptr(allocator), this->m_size);
	}

	uint32 RegionMemoryBlock::size()
	{
		return m_size;
	}


	void RegionAllocator::reserve(uint32 size)
	{
		if (capacity == 0)
		{
			capacity = kib(4);
			while(capacity < size)
			{
				capacity *= 2;
			}
			data = (byte*)malloc(capacity);
		}
		if (capacity - used < size)
		{
			while(capacity - used < size)
			{
				capacity *= 2;
			}
			data = (byte*)realloc((void*)data, capacity);
		}
	}

	RegionMemoryBlock RegionAllocator::popNewBlock(uint32 size)
	{
		for(RegionMemoryBlock& free_block : free_blocks)
		{
			if(free_block.size() >= size && free_block.size() <= size + kib(1))
			{
				RegionMemoryBlock result = free_block;
				free_block = free_blocks.last();
				free_blocks.resize(free_blocks.size() - 1);
				return result;
			}
		}
		RegionMemoryBlock result;
		result.offset = used;
		result.m_size = size;
		used += size;
		return result;
	}

	void RegionAllocator::resize(RegionMemoryBlock& block, uint32 new_size)
	{
		if(new_size == 0)
		{
			free_blocks.push(block);
			block.m_size = 0;
			block.offset = 0;
			return;
		}
		reserve(new_size - block.m_size);

		memcpy(data + used, data + block.offset, block.m_size);
		free_blocks.push(block);
		block = popNewBlock(new_size);
	}

	RegionAllocator& getCurrentAllocator()
	{
		return getCurrentRegion().getAllocator();
	}
}
