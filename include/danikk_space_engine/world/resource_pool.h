#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Finds world resource id.
	 */
	size_t findWorldResourceId(const DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Performs the insert world resource id operation.
	 */
	void insertWorldResourceId(DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Erases world resource id.
	 */
	bool eraseWorldResourceId(DynamicArray<uint32>& ids, uint32 id);

	/**
	 * @brief Logs world resource capacity change.
	 */
	void logWorldResourceCapacityChange(const char* resource_name, const char* array_name,
		size_t old_capacity, size_t new_capacity);

	/* Owns stable indexed slots and the active/free ID lists for one resource type. */

	template<typename resource_t, typename id_t>
	/**
	 * @brief Manages stable resource IDs and reuses released slots.
	 */
	class WorldResourcePool
	{
		const char* resource_name;
		DynamicArray<resource_t> slots;
		DynamicArray<id_t> active_ids;
		DynamicArray<size_t> active_indices;
		DynamicArray<id_t> free_ids;

		// Captures all backing arrays so implicit growth in push/pop/resize is covered too.

		/**
		 * @brief Represents capacity watcher data and behavior.
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

				logWorldResourceCapacityChange(pool.resource_name, "slots", slots_capacity, pool.slots.capacity());

				logWorldResourceCapacityChange(pool.resource_name, "active_ids", active_ids_capacity, pool.active_ids.capacity());

				logWorldResourceCapacityChange(pool.resource_name, "active_indices", active_indices_capacity, pool.active_indices.capacity());

				logWorldResourceCapacityChange(pool.resource_name, "free_ids", free_ids_capacity, pool.free_ids.capacity());
			}
		};

	public:
		// The name is retained by the pool; use a string literal or another static string.
		/**
		 * @brief Initializes an empty pool and assigns its diagnostic name.
		 */
		explicit WorldResourcePool(const char* resource_name) : resource_name(resource_name) {}

		/**
		 * @brief Performs the create operation.
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
		 * @brief Performs the free operation.
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
		 * @brief Reports whether active.
		 */
		bool isActive(id_t id) const
		{
			if(id >= active_indices.size()) return false;
			const size_t index = active_indices[id];

			return index < active_ids.size() && active_ids[index] == id;
		}

		/**
		 * @brief Returns the resource stored at the given ID.
		 */
		resource_t& operator[](id_t id) { return slots[id]; }
		/**
		 * @brief Returns the resource stored at the given ID without allowing modification.
		 */
		const resource_t& operator[](id_t id) const { return slots[id]; }
		/**
		 * @brief Returns the number of allocated resource slots.
		 */
		size_t size() const { return slots.size(); }
		/**
		 * @brief Returns the IDs of resources currently in use.
		 */
		const DynamicArray<id_t>& usedIds() const { return active_ids; }

		/**
		 * @brief Performs the clear operation.
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
		 * @brief Releases memory.
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
