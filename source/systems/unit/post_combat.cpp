#include <cmath>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Removes one stored post-combat assignment while preserving the remaining order. */
	void removePostCombatAssignment(DynamicArray<UnitPostCombatAssignment>& assignments, size_t index)
	{
		for(size_t i = index + 1; i < assignments.size(); ++i)
			assignments[i - 1] = static_cast<UnitPostCombatAssignment&&>(assignments[i]);
		assignments.resize(assignments.size() - 1);
	}

	/** @brief Spreads out the group and scans its surroundings during the reconnaissance phase. */
	void assignPostCombatOrders(World& world, UnitGroup& system)
	{
		DSE_WATCH();
		DynamicArray<UnitPostCombatAssignment>& assignments = system.post_combat_assignments;
		if(!system.has_had_enemy_contact || !system.war_active || system.enemy_seen_this_tick)
		{
			assignments.clear();
			return;
		}

		DynamicArray<UnitSearchCandidate>& candidates = system.search_candidates;
		candidates.clear();
		candidates.reserve(system.agents.size());
		for(UnitAgentState& agent : system.agents)
		{
			PointComponentSlot* slot = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveUnit(world, agent, slot, human) || human->faction_id != system.faction_id) continue;
			const WorldBodyId body_id = world.chunks.getChunk(slot->chunk_id).world_body_id;
			const glm::dvec3 position = world.getPointWorldPosition(slot->chunk_id, slot->point_id);
			candidates.push(UnitSearchCandidate{agent.human_slot_id, slot, &agent, human, body_id, position, false});
		}

		// Drop assignments for removed units, units that moved to another body, and units now fighting/searching.
		for(size_t i = 0; i < assignments.size();)
		{
			UnitPostCombatAssignment& assignment = assignments[i];
			UnitSearchCandidate* candidate = nullptr;
			for(UnitSearchCandidate& current : candidates)
				if(current.slot_id == assignment.unit_slot_id)
				{
					candidate = &current;
					break;
				}
			if(candidate == nullptr || candidate->slot->generation_id != assignment.unit_generation_id
				|| candidate->body_id != assignment.body_id || candidate->agent->order.has_attack_target
				|| candidate->agent->order.has_move_target)
			{
				removePostCombatAssignment(assignments, i);
				continue;
			}
			++i;
		}

		const uint32 now = danikk_engine::getMsTimestamp();
		for(UnitSearchCandidate& candidate : candidates)
		{
			UnitAgentState& agent = *candidate.agent;
			HumanComponent& human = *candidate.human;
			// Last-known enemy locations have priority over general area scanning.
			if(agent.order.has_attack_target || agent.order.has_move_target) continue;
			agent.global_state = UnitGlobalState::war;
			agent.substate = UnitSubstate::reconnaissance;

			UnitPostCombatAssignment* assignment = nullptr;
			for(UnitPostCombatAssignment& existing : assignments)
				if(existing.unit_slot_id == candidate.slot_id
					&& existing.unit_generation_id == candidate.slot->generation_id
					&& existing.body_id == candidate.body_id)
				{
					assignment = &existing;
					break;
				}

			const Chunk& chunk = world.chunks.getChunk(candidate.slot->chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(candidate.body_id);
			const glm::dvec3 body_local_position = chunk.position
				+ glm::dvec3(world.point_data.getPoint(candidate.slot->point_id).position);
			if(assignment == nullptr)
			{
				vec3 up = human.up;
				if(glm::length(up) < 1e-6f) up = vec3(0.0f, 0.0f, 1.0f);
				up = glm::normalize(up);
				const vec3 reference = std::abs(up.x) < 0.8f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
				const vec3 tangent_x = glm::normalize(glm::cross(up, reference));
				const vec3 tangent_y = glm::normalize(glm::cross(up, tangent_x));
				const uint32 seed = static_cast<uint32>(candidate.slot_id) * 2654435761u
					+ static_cast<uint32>(candidate.slot->generation_id);
				const float angle = static_cast<float>(seed % 6283u) * 0.001f;
				const float radius = 2.5f + static_cast<float>((seed >> 8) % 4u) * 0.75f;
				UnitPostCombatAssignment created;
				created.unit_slot_id = candidate.slot_id;
				created.unit_generation_id = candidate.slot->generation_id;
				created.body_id = candidate.body_id;
				created.body_local_position = body_local_position
					+ glm::dvec3((tangent_x * std::cos(angle) + tangent_y * std::sin(angle)) * radius);
				created.scan_angle = angle + 1.2f;
				created.next_scan_timestamp_ms = now + 1800u + seed % 1000u;
				assignments.push(created);
				assignment = &assignments[assignments.size() - 1];
			}

			const glm::dvec3 move_target = body.position + body.rotation * assignment->body_local_position;
			const glm::dvec3 move_offset = move_target - candidate.position;
			if(glm::dot(move_offset, move_offset) > 0.5 * 0.5)
			{
				agent.order.has_move_target = true;
				agent.order.move_target_world = move_target;
			}

			if(now - assignment->next_scan_timestamp_ms < 0x80000000u)
			{
				assignment->scan_angle += 1.7f;
				if(assignment->scan_angle > glm::two_pi<float>())
					assignment->scan_angle -= glm::two_pi<float>();
				assignment->next_scan_timestamp_ms = now + 1800u
					+ (static_cast<uint32>(candidate.slot_id) * 97u) % 900u;
			}

			vec3 up = human.up;
			if(glm::length(up) < 1e-6f) up = vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up);
			const vec3 reference = std::abs(up.x) < 0.8f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
			const vec3 tangent_x = glm::normalize(glm::cross(up, reference));
			const vec3 tangent_y = glm::normalize(glm::cross(up, tangent_x));
			const vec3 look_direction = tangent_x * std::cos(assignment->scan_angle)
				+ tangent_y * std::sin(assignment->scan_angle) + up * 0.18f;
			agent.order.has_look_target = true;
			agent.order.look_target_world = body.position
				+ body.rotation * (body_local_position + glm::dvec3(look_direction * 8.0f));
		}
	}
}
