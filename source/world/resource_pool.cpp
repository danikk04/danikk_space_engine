#include <danikk_framework/log.h>
#include <danikk_space_engine/world/resource_pool.h>

namespace danikk_space_engine
{

	/**
	 * @brief Logs world resource capacity change.
	 */
	void logWorldResourceCapacityChange(const char* resource_name, const char* array_name,
		size_t old_capacity, size_t new_capacity)
	{
		if(old_capacity == new_capacity) return;
		formatLogInfo("WorldData pool % (%): capacity % -> %",
			resource_name, array_name, old_capacity, new_capacity);
	}

	/**
	 * @brief Finds world resource id.
	 */
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

	/**
	 * @brief Adds an ID to the active resource list without creating a duplicate.
	 */
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

	/**
	 * @brief Erases world resource id.
	 */
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
