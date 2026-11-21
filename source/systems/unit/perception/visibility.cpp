#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		void markVisibilityChecked(UnitGroup& system, const UnitTarget& target)
		{
			for(UnitSighting& sighting : system.known_sightings)
			{
				if(sighting.target_slot_id != target.slot_id
					|| sighting.target_generation_id != target.generation_id) continue;
				sighting.visibility_checked_this_tick = true;
				return;
			}
		}
	}

	/** @brief Ages and prunes faction intelligence, then performs the scheduled NPC visibility raycasts. */
	void updatePerception(World& world, UnitGroup& system, const UnitSpatialIndex& spatial_index)
	{
		DSE_WATCH();
		system.enemy_seen_this_tick = false;
		for(UnitSighting& sighting : system.known_sightings)
		{
			sighting.visible_this_tick = false;
			sighting.visibility_checked_this_tick = false;
		}

		for(UnitAgentState& agent : system.agents)
		{
			PointComponentSlot* human_slot = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveUnit(world, agent, human_slot, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			const UnitTarget previous_target{agent.order.target_slot_id,
				agent.order.target_point_id, agent.order.target_generation_id};
			if(agent.order.has_attack_target) markVisibilityChecked(system, previous_target);
			if(agent.order.has_attack_target && isValidEnemy(world, previous_target, human->faction_id))
			{
				const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(previous_target.slot_id);
				const glm::dvec3 origin = world.getPointWorldPosition(human_slot->chunk_id, human_slot->point_id);
				const Chunk& chunk = world.chunks.getChunk(human_slot->chunk_id);
				const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, previous_target.point_id);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance > 0.001)
				{
					const RaycastHit hit = world.raycasts.cast(shot_origin, vec3(delta), static_cast<float>(distance));
					if(!hit.hit || hit.distance >= distance - 0.1)
						rememberSighting(world, system, human->faction_id, previous_target, target_position);
				}
			}
			else
			{
				agent.order.has_attack_target = false;
				if(agent.candidate_index >= agent.enemy_candidates.size())
					collectEnemyTargets(world, human_slot->chunk_id,
						human->faction_id, spatial_index, agent);
				if(agent.candidate_index >= agent.enemy_candidates.size()) continue;
				const UnitTarget candidate = agent.enemy_candidates[agent.candidate_index++];
				if(!isValidEnemy(world, candidate, human->faction_id)) continue;
				markVisibilityChecked(system, candidate);
				const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(candidate.slot_id);
				const glm::dvec3 origin = world.getPointWorldPosition(human_slot->chunk_id, human_slot->point_id);
				const Chunk& chunk = world.chunks.getChunk(human_slot->chunk_id);
				const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, candidate.point_id);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance <= 0.001) continue;
				const RaycastHit hit = world.raycasts.cast(shot_origin, vec3(delta), static_cast<float>(distance));
				if(!hit.hit || hit.distance >= distance - 0.1)
					rememberSighting(world, system, human->faction_id, candidate, target_position);
			}
		}
	}
}
