#include <cmath>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/unit/internal/battle.h>
#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Removes one stored post-combat assignment while preserving the remaining order. */
	void removePostCombatAssignment(DynamicArray<UnitPostCombatAssignment>& assignments, size_t index)
	{
		for(size_t i = index + 1; i < assignments.size(); ++i)
			assignments[i - 1] = static_cast<UnitPostCombatAssignment&&>(assignments[i]);
		assignments.resize(assignments.size() - 1);
	}

	/** @brief Spreads units and scans surroundings while the group searches for hostiles. */
	void assignBattleReconnaissanceOrders(World& world, UnitGroup& system)
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
			HumanData* human = nullptr;
			if(!getActiveUnit(world, agent, human) || human->faction_id != system.faction_id) continue;
			const WorldBodyId body_id = human->world_body_id;
			const glm::dvec3 position = world.getHumanSystem().getWorldPosition(*human);
			candidates.push(UnitSearchCandidate{agent.human_id, human, &agent, human, body_id, position, false});
		}

		// Drop assignments for removed units, units that moved to another body, and units now fighting/searching.
		for(size_t i = 0; i < assignments.size();)
		{
			UnitPostCombatAssignment& assignment = assignments[i];
			UnitSearchCandidate* candidate = nullptr;
			for(UnitSearchCandidate& current : candidates)
				if(current.human_id == assignment.human_id)
				{
					candidate = &current;
					break;
				}
			if(candidate == nullptr || candidate->data->generation_id != assignment.unit_generation_id
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
			HumanState& human = *candidate.human;
			// Last-known enemy locations have priority over general area scanning.
			if(agent.order.has_attack_target || agent.order.has_move_target) continue;
			agent.substate = UnitSubstate::reconnaissance;

			UnitPostCombatAssignment* assignment = nullptr;
			for(UnitPostCombatAssignment& existing : assignments)
				if(existing.human_id == candidate.human_id
					&& existing.unit_generation_id == candidate.data->generation_id
					&& existing.body_id == candidate.body_id)
				{
					assignment = &existing;
					break;
				}

			const WorldBody& body = world.getWorldBodyData().getWorldBody(candidate.body_id);
			const glm::dvec3 body_local_position = candidate.data->body_local_position;
			if(assignment == nullptr)
			{
				vec3 up = human.up;
				if(glm::length(up) < 1e-6f) up = vec3(0.0f, 0.0f, 1.0f);
				up = glm::normalize(up);
				const vec3 reference = std::abs(up.x) < 0.8f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
				const vec3 tangent_x = glm::normalize(glm::cross(up, reference));
				const vec3 tangent_y = glm::normalize(glm::cross(up, tangent_x));
				const uint32 seed = static_cast<uint32>(candidate.human_id) * 2654435761u
					+ static_cast<uint32>(candidate.data->generation_id);
				const float angle = static_cast<float>(seed % 6283u) * 0.001f;
				const float radius = 2.5f + static_cast<float>((seed >> 8) % 4u) * 0.75f;
				UnitPostCombatAssignment created;
				created.human_id = candidate.human_id;
				created.unit_generation_id = candidate.data->generation_id;
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
					+ (static_cast<uint32>(candidate.human_id) * 97u) % 900u;
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
