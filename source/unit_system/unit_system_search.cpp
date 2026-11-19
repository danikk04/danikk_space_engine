#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
	/** @brief Resolves a body's current world-space position for a stored local search point. */
	glm::dvec3 getSearchWorldPosition(const World& world, const UnitSearchLocation& location)
	{
		if(!world.world_body_data.isWorldBodyActive(location.body_id)) return glm::dvec3(std::numeric_limits<double>::max());
		const WorldBody& body = world.world_body_data.getWorldBody(location.body_id);
		return body.position + body.rotation * location.body_local_position;
	}

		/** @brief Checks corpses in one nearby chunk and defers destruction of visible eligible bodies. */
		void inspectChunkCorpses(World& world, const UnitSystem& system,
			const glm::dvec3& search_position,
			const glm::dvec3& eye_position, double search_radius_squared, ChunkId chunk_id)
	{
			if(!world.chunk_system.isChunkActive(chunk_id)) return;
			const Chunk& chunk = world.chunk_system.getChunk(chunk_id);
			for(PointComponentSlotId id : chunk.getHumanComponentSlotIds())
		{
			if(!world.point_data.isPointComponentSlotActive(id)) continue;
			const PointComponentSlot& corpse_slot = world.point_data.getPointComponentSlot(id);
				if(!world.point_data.isPointActive(corpse_slot.point_id)) continue;
			const HumanComponent& corpse = *static_cast<const HumanComponent*>(corpse_slot.getData());
				if(!corpse.hasDeathTimestamp()) continue;
			if(corpse.faction_id >= world.factions.size()
				|| system.faction_id >= world.factions.size()) continue;
			const bool is_ally = corpse.faction_id == system.faction_id;
			const bool is_hostile = world.factions.getRelation(
				system.faction_id, corpse.faction_id).type == FactionRelationType::hostile;
			if(!is_ally && !is_hostile) continue;
			const glm::dvec3 corpse_position = world.getPointWorldPosition(corpse_slot.chunk_id, corpse_slot.point_id);
			const glm::dvec3 search_offset = corpse_position - search_position;
			if(glm::dot(search_offset, search_offset) > search_radius_squared) continue;
				const DamageableComponent* health = findDamageable(world, corpse_slot.chunk_id, corpse_slot.point_id);
				if(health == nullptr || health->getHealth() > 0.0f) continue;
			const glm::dvec3 ray = corpse_position - eye_position;
			const double ray_length = glm::length(ray);
			if(ray_length > 0.001)
			{
				const RaycastHit hit = world.raycast(eye_position, vec3(ray), static_cast<float>(ray_length));
				if(hit.hit && hit.distance < ray_length - 0.1) continue;
			}
			world.deferPointDestruction(corpse_slot.point_id);
		}
	}

		/** @brief Deletes visible allied or hostile corpses close to an inspected search location. */
		void removeCorpsesAtSearchLocation(World& world, const UnitSystem& system,
			const PointComponentSlot& inspector_slot, const HumanComponent& inspector,
			const UnitSearchLocation& location, const glm::dvec3& search_position)
		{
			DSE_WATCH();
			const glm::dvec3 eye_position = world.getPointWorldPosition(inspector_slot.chunk_id, inspector_slot.point_id)
				+ glm::dvec3(inspector.up * (inspector.getHeight() * 0.25f));
			constexpr double search_radius = 2.5;
			constexpr double search_radius_squared = search_radius * search_radius;
			const WorldBody& body = world.world_body_data.getWorldBody(location.body_id);
			const PlanetComponent* planet = nullptr;
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(!world.world_body_data.isWorldBodyComponentSlotActive(id)) continue;
				const WorldBodyComponentSlot& component = world.world_body_data.getWorldBodyComponentSlot(id);
				if(component.type_id != WorldBodyComponentTypeId::planet) continue;
				planet = static_cast<const PlanetComponent*>(component.getData());
				break;
			}

			if(planet == nullptr)
			{
				for(ChunkId chunk_id : body.chunk_ids)
					inspectChunkCorpses(world, system, search_position, eye_position, search_radius_squared, chunk_id);
				return;
			}

			const glm::ivec3 center_coordinate = glm::ivec3(glm::floor(
				location.body_local_position / planet_chunk_width));
			for(int32 z = -1; z <= 1; ++z)
				for(int32 y = -1; y <= 1; ++y)
					for(int32 x = -1; x <= 1; ++x)
					{
						const glm::ivec3 coordinate = center_coordinate + glm::ivec3(x, y, z);
						ChunkId chunk_id;
						if(!planet->chunk_lookup.getChunk(coordinate, chunk_id)
							|| !world.chunk_system.isChunkActive(chunk_id)) continue;
						const Chunk& chunk = world.chunk_system.getChunk(chunk_id);
						if(chunk.world_body_id != location.body_id || chunk.grid_coordinate != coordinate) continue;
						inspectChunkCorpses(world, system, search_position, eye_position, search_radius_squared, chunk_id);
					}
		}

	/** @brief Orders available units to inspect lost-contact locations after ten seconds without sightings. */
	void assignSearchOrders(World& world, UnitSystem& system)
	{
		DSE_WATCH();
		if(!system.has_had_enemy_contact || system.enemy_contact_this_tick
			|| system.time_since_enemy_visible < 10.0f || system.search_locations.size() == 0) return;

		DynamicArray<UnitSearchCandidate>& candidates = system.search_candidates;
		candidates.clear();
		candidates.reserve(system.agents.size());
		for(UnitAgentState& agent : system.agents)
		{
			PointComponentSlot* slot = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveUnit(world, agent, slot, human) || human->faction_id != system.faction_id) continue;
			const WorldBodyId body_id = world.chunk_system.getChunk(slot->chunk_id).world_body_id;
			const glm::dvec3 position = world.getPointWorldPosition(slot->chunk_id, slot->point_id);
			candidates.push(UnitSearchCandidate{agent.human_slot_id, slot, &agent, human, body_id, position, false});
		}

		// Reserve agents already assigned to valid locations before assigning the remaining units.
		for(const UnitSearchLocation& location : system.search_locations)
		{
			if(location.assigned_unit_slot_id == static_cast<PointComponentSlotId>(-1)) continue;
			for(UnitSearchCandidate& candidate : candidates)
			{
				if(candidate.slot_id != location.assigned_unit_slot_id
					|| candidate.body_id != location.body_id) continue;
				candidate.assigned_to_search = true;
				break;
			}
		}

		auto eraseCurrentLocation = [&system](size_t index)
		{
			for(size_t shift = index + 1; shift < system.search_locations.size(); ++shift)
				system.search_locations[shift - 1] = static_cast<UnitSearchLocation&&>(system.search_locations[shift]);
			system.search_locations.resize(system.search_locations.size() - 1);
		};

		for(size_t index = 0; index < system.search_locations.size();)
		{
			UnitSearchLocation& location = system.search_locations[index];
			UnitSearchCandidate* assigned_unit = nullptr;
			if(location.assigned_unit_slot_id != static_cast<PointComponentSlotId>(-1))
			{
				for(UnitSearchCandidate& candidate : candidates)
					if(candidate.slot_id == location.assigned_unit_slot_id)
					{
						assigned_unit = &candidate;
						break;
					}
				if(assigned_unit == nullptr || assigned_unit->body_id != location.body_id)
				{
					if(assigned_unit != nullptr) assigned_unit->assigned_to_search = false;
					location.assigned_unit_slot_id = static_cast<PointComponentSlotId>(-1);
				}
				else if(!assigned_unit->agent->order.has_attack_target
					&& world.world_body_data.isWorldBodyActive(location.body_id))
				{
					const glm::dvec3 target_position = getSearchWorldPosition(world, location);
					const glm::dvec3 offset = target_position - assigned_unit->position;
					if(glm::dot(offset, offset) <= 1.5 * 1.5)
					{
						removeCorpsesAtSearchLocation(world, system, *assigned_unit->slot,
							*assigned_unit->human, location, target_position);
						assigned_unit->assigned_to_search = false;
						eraseCurrentLocation(index);
						continue;
					}
					assigned_unit->agent->order.has_move_target = true;
					assigned_unit->agent->order.move_target_world = target_position;
					assigned_unit->agent->order.has_look_target = true;
					assigned_unit->agent->order.look_target_world = target_position;
					++index;
					continue;
				}
			}

			if(location.assigned_unit_slot_id == static_cast<PointComponentSlotId>(-1))
			{
				const glm::dvec3 target_position = getSearchWorldPosition(world, location);
				UnitSearchCandidate* nearest_candidate = nullptr;
				double nearest_distance_squared = std::numeric_limits<double>::max();
				for(UnitSearchCandidate& candidate : candidates)
				{
					if(candidate.body_id != location.body_id || candidate.agent->order.has_attack_target
						|| candidate.assigned_to_search) continue;
					const glm::dvec3 offset = candidate.position - target_position;
					const double distance_squared = glm::dot(offset, offset);
					if(distance_squared < nearest_distance_squared)
					{
						nearest_distance_squared = distance_squared;
						nearest_candidate = &candidate;
					}
				}
				if(nearest_candidate == nullptr) { ++index; continue; }
				location.assigned_unit_slot_id = nearest_candidate->slot_id;
				nearest_candidate->assigned_to_search = true;
				const glm::dvec3 offset = target_position - nearest_candidate->position;
				if(glm::dot(offset, offset) <= 1.5 * 1.5)
				{
					removeCorpsesAtSearchLocation(world, system, *nearest_candidate->slot,
						*nearest_candidate->human, location, target_position);
					nearest_candidate->assigned_to_search = false;
					eraseCurrentLocation(index);
					continue;
				}
				nearest_candidate->agent->order.has_move_target = true;
				nearest_candidate->agent->order.move_target_world = target_position;
				nearest_candidate->agent->order.has_look_target = true;
				nearest_candidate->agent->order.look_target_world = target_position;
			}
			++index;
		}
		candidates.clear();
	}
}
