#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{
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
		Point& point = points[id];
		if(world.getWorldBodySystem().isWorldBodyActive(point.world_body_id))
		{
			WorldBody& body = world.getWorldBodySystem().getWorldBody(point.world_body_id);
			body.spatial_tree.remove(id, point.body_local_position);
			const size_t index = point.world_body_point_index;
			if(index >= body.point_ids.size() || body.point_ids[index] != id)
				danikk_engine::formatFatalError("Point % has an invalid WorldBody membership index", id);
			for(size_t move = index + 1; move < body.point_ids.size(); ++move)
				body.point_ids[move - 1] = body.point_ids[move];
			body.point_ids.resize(body.point_ids.size() - 1);
			for(size_t move = index; move < body.point_ids.size(); ++move)
				getPoint(body.point_ids[move]).world_body_point_index = move;
			for(PointComponentSlotId slot_id : point.component_slot_ids)
				if(isPointComponentSlotActive(slot_id)
					&& getPointComponentSlot(slot_id).type_id == PointComponentTypeId::build_part)
					world.getMeshGenerationSystem().invalidateBodyDetailMesh(point.world_body_id);
		}
		DynamicArray<PointComponentSlotId>& attached_slots = points[id].component_slot_ids;
		while(attached_slots.size() > 0)
			destroyPointComponentSlot(attached_slots[attached_slots.size() - 1]);
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
