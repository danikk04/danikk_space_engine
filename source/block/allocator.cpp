#include <block/allocator.h>
#include <object/block_map.h>
#include <block/context.h>

namespace danikk_space_engine
{
	void MonolithMemoryBlock::resize(uint32 new_size, MonolithAllocator& allocator)
	{
		allocator.resize(*this, new_size);
	}

	void MonolithMemoryBlock::free(MonolithAllocator& allocator)
	{
		if(m_size != 0)
		{
			allocator.free_blocks.push(*this);
			offset = m_size = 0;
		}
	}

	void MonolithMemoryBlock::nullify(MonolithAllocator& allocator)
	{
		offset = m_size = 0;
	}

	byte* MonolithMemoryBlock::ptr(MonolithAllocator& allocator)
	{
		return allocator.data + offset;
	}

	const byte* MonolithMemoryBlock::ptr(MonolithAllocator& allocator) const
	{
		return allocator.data + offset;
	}

	void MonolithMemoryBlock::copyTo(MonolithMemoryBlock& other, MonolithAllocator& allocator) const
	{
		other.resize(this->m_size, allocator);
		memcpy(other.ptr(allocator), ptr(allocator), this->m_size);
	}

	uint32 MonolithMemoryBlock::size()
	{
		return m_size;
	}


	void MonolithAllocator::reserve(uint32 size)
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

	MonolithMemoryBlock MonolithAllocator::popNewBlock(uint32 size)
	{
		for(MonolithMemoryBlock& free_block : free_blocks)
		{
			if(free_block.size() >= size && free_block.size() <= size + kib(1))
			{
				MonolithMemoryBlock result = free_block;
				free_block = free_blocks.last();
				free_blocks.resize(free_blocks.size() - 1);
				return result;
			}
		}
		MonolithMemoryBlock result;
		result.offset = used;
		result.m_size = size;
		used += size;
		return result;
	}

	void MonolithAllocator::resize(MonolithMemoryBlock& block, uint32 new_size)
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

	MonolithAllocator& getCurrentAllocator()
	{
		return current_block_context->region->getAllocator();
	}
}
