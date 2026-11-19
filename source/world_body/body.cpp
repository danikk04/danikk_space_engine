#include <danikk_space_engine/world_body.h>

namespace danikk_space_engine
{

	/**
	 * @brief Adds a chunk ID unless it is already present.
	 * @param chunk_id ID to add to this body's ordered membership list.
	 * @details Scans the current list linearly. If no match exists, appends the ID;
	 * repeated additions leave both contents and order unchanged.
	 */
	void WorldBody::addChunk(ChunkId chunk_id)
	{
		for(ChunkId existing_chunk_id : chunk_ids)
		{
			if(existing_chunk_id == chunk_id) return;
		}
		chunk_ids.push(chunk_id);
	}

	/**
	 * @brief Removes the first matching chunk ID without reordering the rest.
	 * @param chunk_id ID to remove from this body's membership list.
	 * @return True when an entry was found and removed; false when absent.
	 * @details After finding the entry, shifts following IDs one position left and
	 * shrinks the array, preserving the relative order of remaining chunks.
	 */
	bool WorldBody::removeChunk(ChunkId chunk_id)
	{
		for(size_t index = 0; index < chunk_ids.size(); index++)
		{
			if(chunk_ids[index] != chunk_id) continue;
			for(size_t shift_index = index; shift_index + 1 < chunk_ids.size(); shift_index++)
			{
				chunk_ids[shift_index] = chunk_ids[shift_index + 1];
			}
			chunk_ids.resize(chunk_ids.size() - 1);
			return true;
		}
		return false;
	}

	/**
	 * @brief Adds a component slot ID unless it is already present.
	 * @param slot_id ID to add to this body's ordered component list.
	 * @details Scans the current list linearly. If no match exists, appends the ID;
	 * repeated additions leave both contents and order unchanged.
	 */
	void WorldBody::addComponentSlot(WorldBodyComponentSlotId slot_id)
	{
		for(WorldBodyComponentSlotId existing_id : component_slot_ids)
		{
			if(existing_id == slot_id) return;
		}
		component_slot_ids.push(slot_id);
	}

	/**
	 * @brief Removes the first matching component slot ID without reordering others.
	 * @param slot_id ID to remove from this body's component list.
	 * @return True when an entry was found and removed; false when absent.
	 * @details Shifts later IDs left after a match, then shrinks the array while
	 * preserving the order of the remaining component slots.
	 */
	bool WorldBody::removeComponentSlot(WorldBodyComponentSlotId slot_id)
	{
		for(size_t index = 0; index < component_slot_ids.size(); index++)
		{
			if(component_slot_ids[index] != slot_id) continue;
			for(size_t shift_index = index; shift_index + 1 < component_slot_ids.size(); shift_index++)
			{
				component_slot_ids[shift_index] = component_slot_ids[shift_index + 1];
			}
			component_slot_ids.resize(component_slot_ids.size() - 1);
			return true;
		}
		return false;
	}
}
