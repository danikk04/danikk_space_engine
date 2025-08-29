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
		other.resize(this-> m_size, allocator);
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

	void RegionAllocator::resize(RegionMemoryBlock& block, uint32 new_size)
	{
		reserve(new_size - block.m_size);

		memcpy(data + used, data + block.offset, block.m_size);
		block.free(*this);

		block.offset = used;
		block.m_size = new_size;
		used += new_size;
	}

	RegionAllocator& getCurrentAllocator()
	{
		return current_region->getAllocator();
	}
}
