#include <danikk_space_engine/world/world_resource_pool.h>

namespace danikk_space_engine
{
	size_t findWorldResourceId(const DynamicArray<uint32>& ids, uint32 id)
	{
		size_t first = 0;
		size_t last = ids.size();
		while(first < last)
		{
			const size_t middle = first + (last - first) / 2;
			if(ids[middle] < id) first = middle + 1;
			else last = middle;
		}
		return first;
	}

	void insertWorldResourceId(DynamicArray<uint32>& ids, uint32 id)
	{
		const size_t index = findWorldResourceId(ids, id);
		ids.resize(ids.size() + 1);
		for(size_t shift_index = ids.size() - 1; shift_index > index; shift_index--)
		{
			ids[shift_index] = ids[shift_index - 1];
		}
		ids[index] = id;
	}

	bool eraseWorldResourceId(DynamicArray<uint32>& ids, uint32 id)
	{
		const size_t index = findWorldResourceId(ids, id);
		if(index == ids.size() || ids[index] != id) return false;
		for(size_t shift_index = index; shift_index + 1 < ids.size(); shift_index++)
		{
			ids[shift_index] = ids[shift_index + 1];
		}
		ids.resize(ids.size() - 1);
		return true;
	}
}
