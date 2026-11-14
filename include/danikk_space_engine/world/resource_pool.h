#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	size_t findWorldResourceId(const DynamicArray<uint32>& ids, uint32 id);
	void insertWorldResourceId(DynamicArray<uint32>& ids, uint32 id);
	bool eraseWorldResourceId(DynamicArray<uint32>& ids, uint32 id);

	/* Owns stable indexed slots and the active/free ID lists for one resource type. */
	template<typename resource_t, typename id_t>
	class WorldResourcePool
	{
		DynamicArray<resource_t> slots;
		DynamicArray<id_t> active_ids;
		DynamicArray<size_t> active_indices;
		DynamicArray<id_t> free_ids;

	public:
		id_t create()
		{
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

		bool free(id_t id)
		{
			if(!isActive(id)) return false;
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

		bool isActive(id_t id) const
		{
			if(id >= active_indices.size()) return false;
			const size_t index = active_indices[id];
			return index < active_ids.size() && active_ids[index] == id;
		}

		resource_t& operator[](id_t id) { return slots[id]; }
		const resource_t& operator[](id_t id) const { return slots[id]; }
		size_t size() const { return slots.size(); }
		const DynamicArray<id_t>& usedIds() const { return active_ids; }

		void clear()
		{
			slots.clear();
			active_ids.clear();
			active_indices.clear();
			free_ids.clear();
		}

		void releaseMemory()
		{
			clear();
			slots = DynamicArray<resource_t>();
			active_ids = DynamicArray<id_t>();
			active_indices = DynamicArray<size_t>();
			free_ids = DynamicArray<id_t>();
		}
	};
}
