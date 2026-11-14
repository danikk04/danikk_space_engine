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
		DynamicArray<id_t> free_ids;

	public:
		id_t create()
		{
			if(free_ids.size() > 0)
			{
				const id_t id = free_ids.pop();
				slots[id] = resource_t();
				insertWorldResourceId(active_ids, static_cast<uint32>(id));
				return id;
			}
			const id_t id = static_cast<id_t>(slots.size());
			slots.pushCtor();
			insertWorldResourceId(active_ids, static_cast<uint32>(id));
			return id;
		}

		bool free(id_t id)
		{
			if(!isActive(id)) return false;
			eraseWorldResourceId(active_ids, static_cast<uint32>(id));
			free_ids.push(id);
			return true;
		}

		bool isActive(id_t id) const
		{
			const size_t index = findWorldResourceId(active_ids, static_cast<uint32>(id));
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
			free_ids.clear();
		}

		void releaseMemory()
		{
			clear();
			slots = DynamicArray<resource_t>();
			active_ids = DynamicArray<id_t>();
			free_ids = DynamicArray<id_t>();
		}
	};
}
