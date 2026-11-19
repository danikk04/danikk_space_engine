#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/planet/planet.h>

#include "internal.h"

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
	/** @brief Rebuilds a unit's nearby hostile-human candidates from its loaded 3x3x3 chunk neighborhood. */
	void collectEnemyTargets(World& world, PointId point_id, ChunkId chunk_id, FactionId faction,
		UnitAgentState& agent)
	{
		DSE_WATCH();
		agent.enemy_candidates.clear();
		agent.candidate_index = 0;
		const Chunk& owner = world.chunks.getChunk(chunk_id);
		const glm::ivec3 coordinate = glm::ivec3(glm::floor(
			(owner.position + glm::dvec3(world.point_data.getPoint(point_id).position)) / planet_chunk_width));
		const PlanetData* planet = world.planets.findPlanet(owner.world_body_id);
		auto collect = [&](ChunkId candidate_chunk)
		{
			if(!world.chunks.isChunkActive(candidate_chunk)) return;
			for(PointComponentSlotId id : world.chunks.getChunk(candidate_chunk).point_component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(id);
				if(slot.type_id != PointComponentTypeId::human || slot.point_id == point_id) continue;
				const UnitTarget target{id, slot.point_id, slot.generation_id};
				if(isValidEnemy(world, target, faction)) agent.enemy_candidates.push(target);
			}
		};
		if(planet == nullptr) { collect(chunk_id); return; }
		for(int x = -1; x <= 1; ++x)
			for(int y = -1; y <= 1; ++y)
				for(int z = -1; z <= 1; ++z)
				{
					ChunkId nearby = static_cast<ChunkId>(-1);
					if(planet->chunk_lookup.getChunk(coordinate + glm::ivec3(x, y, z), nearby)) collect(nearby);
				}
	}
	/** @brief Ages and prunes faction intelligence, then performs the scheduled NPC visibility raycasts. */
	void updatePerception(World& world, UnitGroup& system)
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
					collectEnemyTargets(world, human_slot->point_id,
						human_slot->chunk_id, human->faction_id, agent);
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
