#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		constexpr double unit_spatial_query_half_extent = 64.0;
	}

	/** @brief Rebuilds the flat tactical index from HumanSystem's authoritative array. */
	void rebuildUnitSpatialIndex(World& world, UnitSpatialIndex& index)
	{
		DSE_WATCH();
		index.entries.clear();
		for(const HumanData& human : world.getHumanSystem().getHumans())
		{
			if(!world.getWorldBodySystem().isWorldBodyActive(human.world_body_id)) continue;
			UnitSpatialEntry& entry = index.entries.pushCtor();
			entry.target = UnitTarget{human.id, human.generation_id};
			entry.body_id = human.world_body_id;
			entry.position = human.body_local_position;
		}
	}

	/** @brief Collects nearby hostile humans from the flat HumanSystem array. */
	void collectEnemyTargets(World& world, WorldBodyId body_id, FactionId faction,
		const UnitSpatialIndex&, UnitAgentState& agent)
	{
		DSE_WATCH();
		agent.enemy_candidates.clear();
		agent.candidate_index = 0;
		const HumanData* own = world.getHumanSystem().findHuman(agent.human_id);
		if(own == nullptr || own->generation_id != agent.human_generation_id
			|| own->world_body_id != body_id) return;
		const glm::dvec3 extent(unit_spatial_query_half_extent);
		const glm::dvec3 minimum = own->body_local_position - extent;
		const glm::dvec3 maximum = own->body_local_position + extent;
		for(const HumanData& candidate : world.getHumanSystem().getHumans())
		{
			if(candidate.world_body_id != body_id) continue;
			if(glm::any(glm::lessThan(candidate.body_local_position, minimum))
				|| glm::any(glm::greaterThan(candidate.body_local_position, maximum))) continue;
			const UnitTarget target{candidate.id, candidate.generation_id};
			if(isValidEnemy(world, target, faction)) agent.enemy_candidates.push(target);
		}
	}

	/** @brief Collects humans within an axis-aligned body-local range. */
	void collectNearbyHumanIds(World& world, const UnitSpatialIndex&, WorldBodyId body_id,
		const glm::dvec3& center, double half_extent,
		DynamicArray<HumanId>& result)
	{
		DSE_WATCH();
		result.clear();
		if(half_extent < 0.0 || !world.getWorldBodySystem().isWorldBodyActive(body_id)) return;
		const glm::dvec3 extent(half_extent);
		for(const HumanData& human : world.getHumanSystem().getHumans())
		{
			if(human.world_body_id != body_id) continue;
			const glm::dvec3 delta = glm::abs(human.body_local_position - center);
			if(delta.x <= extent.x && delta.y <= extent.y && delta.z <= extent.z)
				result.push(human.id);
		}
	}

	/** @brief Copies human IDs and owning bodies from the tactical index. */
	void collectHumanIds(const UnitSpatialIndex& index,
		DynamicArray<HumanId>& human_ids, DynamicArray<WorldBodyId>& body_ids)
	{
		DSE_WATCH();
		human_ids.clear();
		body_ids.clear();
		for(const UnitSpatialEntry& entry : index.entries)
		{
			human_ids.push(entry.target.human_id);
			body_ids.push(entry.body_id);
		}
	}
}
