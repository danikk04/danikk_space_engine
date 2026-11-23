#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		void markVisibilityChecked(UnitGroup& system, const UnitTarget& target)
		{
			for(UnitSighting& sighting : system.known_sightings)
			{
				if(sighting.target_human_id != target.human_id
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
			HumanData* human = nullptr;
			if(!getActiveUnit(world, agent, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			const UnitTarget previous_target{agent.order.target_human_id,
				agent.order.target_generation_id};
			if(agent.order.has_attack_target) markVisibilityChecked(system, previous_target);
			if(agent.order.has_attack_target && isValidEnemy(world, previous_target, human->faction_id))
			{
				const glm::dvec3 origin = world.getHumanSystem().getWorldPosition(*human);
				const WorldBody& body = world.getWorldBodyData().getWorldBody(human->world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const HumanData* target_human = world.getHumanSystem().findHuman(previous_target.human_id);
				if(target_human == nullptr) continue;
				const glm::dvec3 target_position = world.getHumanSystem().getWorldPosition(*target_human);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance > 0.001)
				{
					const RaycastHit hit = world.getRaycastSystem().cast(shot_origin, vec3(delta), static_cast<float>(distance));
					if(!hit.hit || hit.distance >= distance - 0.1)
						rememberSighting(world, system, human->faction_id, previous_target, target_position);
				}
			}
			else
			{
				agent.order.has_attack_target = false;
				if(agent.candidate_index >= agent.enemy_candidates.size())
					collectEnemyTargets(world, human->world_body_id,
						human->faction_id, spatial_index, agent);
				if(agent.candidate_index >= agent.enemy_candidates.size()) continue;
				const UnitTarget candidate = agent.enemy_candidates[agent.candidate_index++];
				if(!isValidEnemy(world, candidate, human->faction_id)) continue;
				markVisibilityChecked(system, candidate);
				const glm::dvec3 origin = world.getHumanSystem().getWorldPosition(*human);
				const WorldBody& body = world.getWorldBodyData().getWorldBody(human->world_body_id);
				const glm::dvec3 shot_origin = origin + body.rotation
					* glm::dvec3(human->up * (human->getHeight() * 0.2f));
				const HumanData* target_human = world.getHumanSystem().findHuman(candidate.human_id);
				if(target_human == nullptr) continue;
				const glm::dvec3 target_position = world.getHumanSystem().getWorldPosition(*target_human);
				const glm::dvec3 delta = target_position - shot_origin;
				const double distance = glm::length(delta);
				if(distance <= 0.001) continue;
				const RaycastHit hit = world.getRaycastSystem().cast(shot_origin, vec3(delta), static_cast<float>(distance));
				if(!hit.hit || hit.distance >= distance - 0.1)
					rememberSighting(world, system, human->faction_id, candidate, target_position);
			}
		}
	}
}
