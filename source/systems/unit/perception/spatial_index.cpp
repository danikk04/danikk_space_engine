#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		constexpr double unit_spatial_query_half_extent = 64.0;

		void appendHumanSlots(World& world, PointId point_id,
			DynamicArray<PointComponentSlotId>& output)
		{
			if(!world.point_data.isPointActive(point_id)) return;
			for(PointComponentSlotId slot_id : world.point_data.getPoint(point_id).component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				if(world.point_data.getPointComponentSlot(slot_id).type_id == PointComponentTypeId::human)
					output.push(slot_id);
			}
		}
	}

	/** @brief Refreshes live unit positions in their owning body's shared octree and flat identity list. */
	void rebuildUnitSpatialIndex(World& world, UnitSpatialIndex& index)
	{
		DSE_WATCH();
		index.entries.clear();
		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
			PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
			if(slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.world_body_data.isWorldBodyActive(slot.world_body_id)) continue;
			Point& point = world.point_data.getPoint(slot.point_id);
			const glm::dvec3 local_position = point.body_local_position;
			UnitSpatialEntry& entry = index.entries.pushCtor();
			entry.target = UnitTarget{slot_id, slot.point_id, slot.generation_id};
			entry.body_id = slot.world_body_id;
			entry.position = local_position;
		}
	}

	/** @brief Collects nearby hostile humans from the body's adaptive octree. */
	void collectEnemyTargets(World& world, WorldBodyId body_id, FactionId faction,
		const UnitSpatialIndex&, UnitAgentState& agent)
	{
		DSE_WATCH();
		agent.enemy_candidates.clear();
		agent.candidate_index = 0;
		if(!world.world_body_data.isWorldBodyActive(body_id)
			|| !world.point_data.isPointComponentSlotActive(agent.human_slot_id)) return;
		const PointComponentSlot& agent_slot = world.point_data.getPointComponentSlot(agent.human_slot_id);
		if(agent_slot.generation_id != agent.human_generation_id
			|| !world.point_data.isPointActive(agent_slot.point_id)) return;
		const WorldBody& body = world.world_body_data.getWorldBody(body_id);
		DynamicArray<PointId> point_ids;
		body.spatial_tree.collectBoxCandidates(
			world.point_data.getPoint(agent_slot.point_id).body_local_position,
			unit_spatial_query_half_extent, point_ids);
		for(PointId point_id : point_ids)
		{
			if(!world.point_data.isPointActive(point_id)) continue;
			const Point& point = world.point_data.getPoint(point_id);
			for(PointComponentSlotId slot_id : point.component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::human) continue;
				const UnitTarget target{slot_id, point_id, slot.generation_id};
				if(isValidEnemy(world, target, faction)) agent.enemy_candidates.push(target);
			}
		}
	}

	/** @brief Collects human component slots in a body-local box using the shared WorldBody octree. */
	void collectNearbyHumanSlots(World& world, const UnitSpatialIndex&, WorldBodyId body_id,
		const glm::dvec3& center, double half_extent,
		DynamicArray<PointComponentSlotId>& result)
	{
		DSE_WATCH();
		result.clear();
		if(half_extent < 0.0 || !world.world_body_data.isWorldBodyActive(body_id)) return;
		DynamicArray<PointId> point_ids;
		world.world_body_data.getWorldBody(body_id).spatial_tree.collectBoxCandidates(center, half_extent, point_ids);
		for(PointId point_id : point_ids) appendHumanSlots(world, point_id, result);
	}

	/** @brief Copies current human slot and owning-body pairs from the compact identity list. */
	void collectHumanSlots(const UnitSpatialIndex& index,
		DynamicArray<PointComponentSlotId>& slot_ids, DynamicArray<WorldBodyId>& body_ids)
	{
		DSE_WATCH();
		slot_ids.clear();
		body_ids.clear();
		for(const UnitSpatialEntry& entry : index.entries)
		{
			slot_ids.push(entry.target.slot_id);
			body_ids.push(entry.body_id);
		}
	}
}
