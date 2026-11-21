#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/component/point/build_part.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Destroys every active resource satisfying a predicate, tolerating swap removal. */
		template<typename pool_t, typename predicate_t, typename destroy_t>
		void destroyMatchingResources(pool_t& pool, predicate_t matches, destroy_t destroy)
		{
			for(size_t index = 0; index < pool.usedIds().size();)
			{
				const auto id = pool.usedIds()[index];
				if(matches(pool[id])) destroy(id);
				else index++;
			}
		}

		/** @brief Repeatedly destroys the last active ID until the owning list is empty. */
		template<typename list_t, typename destroy_t>
		void destroyLastUntilEmpty(const list_t& ids, destroy_t destroy)
		{
			while(ids.size() > 0) destroy(ids[ids.size() - 1]);
		}

		/** @brief Clears each supplied pool while retaining its allocated capacity. */
		template<typename... pool_t>
		void clearPools(pool_t&... pools) { (pools.clear(), ...); }

		template<typename... pool_t>
		/** @brief Releases the backing storage of each supplied pool. */
		void releasePools(pool_t&... pools) { (pools.releaseMemory(), ...); }

		/** @brief Invokes a component destructor and frees external payload storage if present. */
		template<typename slot_t, typename component_type_t, typename id_t>
		void destroyComponentPayload(slot_t& slot, const component_type_t& type, id_t id, const char* component_name)
		{
			if(type.destroy == nullptr)
				danikk_engine::formatFatalError("% component type % has no destroy function", component_name, static_cast<uint32>(slot.type_id));
			if(slot.uses_external_data && slot.external_data == nullptr)
				danikk_engine::formatFatalError("% component slot % has no external data", component_name, id);
			type.destroy(slot.getData());
			if(slot.uses_external_data)
			{
				free(slot.external_data);
				slot.external_data = nullptr;
			}
		}
	}

	/** @copydoc PointData::destroyPoint */
	void PointData::destroyPoint(PointId id)
	{
		DSE_WATCH();
		if(!isPointActive(id)) return;
		destroyMatchingResources(point_components,
			[id](const PointComponentSlot& slot) { return slot.point_id == id; },
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		points.free(id);
	}

	/** @copydoc PointData::destroyPointComponentSlot */
	void PointData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;
		const PointComponentSlot& destroyed_slot = point_components[id];
		if(destroyed_slot.type_id == PointComponentTypeId::build_part)
		{
			for(PointComponentSlotId other_id : point_components.usedIds())
				if(other_id != id && isPointComponentSlotActive(other_id))
				{
					PointComponentSlot& other_slot = point_components[other_id];
					if(other_slot.type_id != PointComponentTypeId::build_part) continue;
					BuildPartComponent& child = *static_cast<BuildPartComponent*>(other_slot.getData());
					if(child.parent_slot_id == id) child.parent_slot_id = no_build_part_parent;
				}
		}
		if(isPointActive(destroyed_slot.point_id))
		{
			DynamicArray<PointComponentSlotId>& point_slots = points[destroyed_slot.point_id].component_slot_ids;
			for(size_t index = 0; index < point_slots.size(); ++index)
			{
				if(point_slots[index] != id) continue;
				for(size_t shift = index + 1; shift < point_slots.size(); ++shift)
					point_slots[shift - 1] = point_slots[shift];
				point_slots.resize(point_slots.size() - 1);
				break;
			}
		}
		PointComponentSlot& slot = point_components[id];
		const PointComponentType& type = getPointComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "Point");
		slot.dispose();
		point_components.free(id);
	}

	/** @copydoc PointData::clear */
	void PointData::clear()
	{
		destroyLastUntilEmpty(point_components.usedIds(),
			[this](PointComponentSlotId id) { destroyPointComponentSlot(id); });
		clearPools(point_components, points);
	}

	/** @copydoc PointData::release */
	void PointData::release()
	{
		clear();
		releasePools(points, point_components);
	}
}
