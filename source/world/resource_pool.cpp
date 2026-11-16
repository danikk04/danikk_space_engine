#include <danikk_framework/log.h>
#include <danikk_space_engine/world/resource_pool.h>

namespace danikk_space_engine
{

	/**
	 * @brief Emits one info log for an actual pool-array capacity change.
	 * @param resource_name Name of the resource pool.
	 * @param array_name Name of the pool's backing array.
	 * @param old_capacity Capacity measured before the operation.
	 * @param new_capacity Capacity measured after the operation.
	 */
	void logWorldResourceCapacityChange(const char* resource_name, const char* array_name,
		size_t old_capacity, size_t new_capacity)
	{
		if(old_capacity == new_capacity) return;
		formatLogInfo("WorldData pool % (%): capacity % -> %",
			resource_name, array_name, old_capacity, new_capacity);
	}

	/**
	 * @brief Performs a lower-bound binary search in a sorted list of resource IDs.
	 * @param ids Sorted IDs to search.
	 * @param id Requested ID.
	 * @return Matching index, or the position where the ID can be inserted to retain sorting.
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
	 * @brief Inserts a caller-validated unique ID into its sorted position.
	 * @param ids Sorted ID array to grow and shift.
	 * @param id ID to insert; this function does not check for duplicates.
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
	 * @brief Finds and removes one matching ID while keeping the array sorted.
	 * @param ids Sorted array to modify.
	 * @param id ID to remove.
	 * @return True if the ID was present; false if no matching entry exists.
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
