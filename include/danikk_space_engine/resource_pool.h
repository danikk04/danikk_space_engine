#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	/** @brief Enables Resource Pool backing-array capacity-change logs at compile time. */
	inline constexpr bool log_resource_pool_capacity_changes = false;

	/**
	 * @brief Searches a compact resource-ID list and returns the matching index.
	 *
	 * Uses lower-bound binary search, so the input list must already be sorted in
	 * ascending order.
	 * @param ids ID array to scan.
	 * @param id Value to find.
	 * @return Index of the matching entry, or the insertion position that preserves sorted order.
	 */
	size_t findWorldResourceId(const DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Inserts an ID into a sorted resource list at its lower-bound position.
	 *
	 * Performs a binary search and then shifts later entries to make room; lookup
	 * is logarithmic, but insertion is linear in the number of IDs.
	 * @param ids Sorted ID array to modify.
	 * @param id ID to insert; callers must ensure it is not already present.
	 */
	void insertWorldResourceId(DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Removes the first matching resource ID while preserving the order of remaining entries.
	 *
	 * Uses lower-bound binary search followed by a linear left shift after a match.
	 * @param ids ID array to modify.
	 * @param id Value to remove.
	 * @return True if a matching entry was erased.
	 */
	bool eraseWorldResourceId(DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Logs a pool-array capacity change when the old and new capacities differ.
	 * @param resource_name Pool's diagnostic name, such as `Chunk`.
	 * @param array_name Internal array whose capacity changed.
	 * @param old_capacity Capacity captured before the operation.
	 * @param new_capacity Capacity observed after the operation.
	 */
	void logWorldResourceCapacityChange(const char* resource_name, const char* array_name,
		size_t old_capacity, size_t new_capacity);

	/**
	 * @brief Owns stable indexed slots and active/free ID lists for one resource type.
	 *
	 * IDs are slot indices and remain associated with the same slot until it is freed. Active IDs
	 * are kept densely for iteration, with `active_indices` providing O(1) membership checks; freed
	 * IDs are reused from `free_ids`. `free()` only changes pool bookkeeping: the owner must first
	 * destroy resource-specific payloads and detach references. Iteration order is not stable after
	 * a free because the active list uses swap-remove. `size()` counts all slots, including reusable
	 * ones. Mutating operations log capacity changes for each backing array.
	 * @tparam resource_t Resource stored in each stable slot.
	 * @tparam id_t Unsigned ID type used to expose slot indices.
	 */
	template<typename resource_t, typename id_t>
	class WorldResourcePool
	{
		/** @brief Diagnostic resource name retained for capacity-change logs. */
		const char* resource_name;
		/** @brief Stable indexed resource storage, including currently free slots. */
		DynamicArray<resource_t> slots;
		/** @brief Dense list of currently active IDs; its order changes when entries are freed. */
		DynamicArray<id_t> active_ids;
		/** @brief Reverse map from slot ID to active_ids index, or invalid sentinel for free slots. */
		DynamicArray<size_t> active_indices;
		/** @brief IDs available for reuse by create(). */
		DynamicArray<id_t> free_ids;

		/**
		 * @brief Captures pool array capacities and logs any differences at scope exit.
		 *
		 * Used by pool mutations so early returns after construction are also accounted
		 * for. It observes capacity changes only; it does not affect pool state.
		 */
		struct CapacityWatcher
		{
			const WorldResourcePool& pool;
			size_t slots_capacity;
			size_t active_ids_capacity;
			size_t active_indices_capacity;
			size_t free_ids_capacity;

			explicit CapacityWatcher(const WorldResourcePool& pool)
				: pool(pool), slots_capacity(pool.slots.capacity()),
				  active_ids_capacity(pool.active_ids.capacity()),
				  active_indices_capacity(pool.active_indices.capacity()),
				  free_ids_capacity(pool.free_ids.capacity()) {}
			~CapacityWatcher()
			{
				if constexpr(log_resource_pool_capacity_changes)
				{
					logWorldResourceCapacityChange(
						pool.resource_name, "slots", slots_capacity, pool.slots.capacity());
					logWorldResourceCapacityChange(
						pool.resource_name, "active_ids", active_ids_capacity, pool.active_ids.capacity());
					logWorldResourceCapacityChange(
						pool.resource_name, "active_indices", active_indices_capacity, pool.active_indices.capacity());
					logWorldResourceCapacityChange(
						pool.resource_name, "free_ids", free_ids_capacity, pool.free_ids.capacity());
				}
			}
		};

	public:
		/**
		 * @brief Initializes an empty pool and assigns the name used in capacity logs.
		 *
		 * The pool retains the pointer without copying the text, so callers must provide
		 * a string whose lifetime exceeds the pool's lifetime (normally a string literal).
		 * @param resource_name Stable diagnostic string; normally a string literal.
		 */
		explicit WorldResourcePool(const char* resource_name) : resource_name(resource_name) {}

		/**
		 * @brief Reuses the last free slot or appends storage, then adds its ID to the active set.
		 * @return ID of a default-initialized resource slot.
		 */
		id_t create()
		{

			CapacityWatcher capacity_watcher(*this);

			if(free_ids.size() > 0)
			{
				const id_t id = free_ids.pop();
				slots[id] = resource_t();
				active_indices[id] = active_ids.size();

				active_ids.push(id);
				return id;
			}
			const id_t id = static_cast<id_t>(slots.size());

			slots.pushCtor();

			active_indices.push(active_ids.size());

			active_ids.push(id);
			return id;
		}

		/**
		 * @brief Removes an active ID in O(1) using swap-remove and makes its slot reusable.
		 *
		 * The final active ID is moved into the removed ID's position, so active-list
		 * order can change. This changes pool membership and free-list bookkeeping;
		 * it does not destroy the slot value. Callers must perform resource-specific
		 * cleanup before freeing an ID.
		 * @param id Slot ID to free.
		 * @return True if the ID was active; false if it was invalid or already free.
		 */
		bool free(id_t id)
		{

			if(!isActive(id)) return false;

			CapacityWatcher capacity_watcher(*this);
			const size_t active_index = active_indices[id];
			const size_t last_index = active_ids.size() - 1;
			if(active_index != last_index)
			{
				const id_t moved_id = active_ids[last_index];
				active_ids[active_index] = moved_id;
				active_indices[moved_id] = active_index;
			}

			active_ids.resize(last_index);
			active_indices[id] = static_cast<size_t>(-1);

			free_ids.push(id);
			return true;
		}

		/**
		 * @brief Checks the reverse index to determine whether an ID is currently active.
		 * @param id Slot ID to check.
		 * @return True when the reverse index maps back to this ID in `active_ids`.
		 */
		bool isActive(id_t id) const
		{
			if(id >= active_indices.size()) return false;
			const size_t index = active_indices[id];

			return index < active_ids.size() && active_ids[index] == id;
		}

		/**
		 * @brief Returns a slot by ID without checking that the slot is active.
		 * @param id Slot index; the caller must ensure it is within `size()`.
		 * @return Mutable reference, including when the slot is currently free.
		 */
		resource_t& operator[](id_t id) { return slots[id]; }
		/** @brief Returns a read-only slot by ID without checking that it is active. */
		const resource_t& operator[](id_t id) const { return slots[id]; }
		/** @brief Returns total slot-array length, including slots currently in the free list. */
		size_t size() const { return slots.size(); }
		/**
		 * @brief Returns the dense list of active IDs for iteration.
		 * @return Pool-owned array reference; do not retain it across create(), free(), clear(), or releaseMemory().
		 */
		const DynamicArray<id_t>& usedIds() const { return active_ids; }

		/**
		 * @brief Destroys all active resource objects and empties bookkeeping arrays while retaining capacity.
		 */
		void clear()
		{

			CapacityWatcher capacity_watcher(*this);

			slots.clear();

			active_ids.clear();

			active_indices.clear();

			free_ids.clear();
		}

		/**
		 * @brief Clears resources and replaces all backing arrays to release their capacity.
		 */
		void releaseMemory()
		{

			CapacityWatcher capacity_watcher(*this);
			clear();
			slots = DynamicArray<resource_t>();
			active_ids = DynamicArray<id_t>();
			active_indices = DynamicArray<size_t>();
			free_ids = DynamicArray<id_t>();
		}
	};
}
