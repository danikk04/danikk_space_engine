#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/item/definition.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
	InventoryComponent* findInventory(World& world, ChunkId chunk_id, PointId point_id);

	namespace
	{
		/** @brief Current-tick assignments used to spread a faction's fire across valid targets. */
		struct TargetAllocation
		{
			PointComponentSlotId target_slot_id = static_cast<PointComponentSlotId>(-1);
			uint64 target_generation_id = 0;
			uint32 unit_count = 0;
			float expected_damage_per_second = 0.0f;
		};

		/** @brief Finds the first weapon stack in an NPC's inventory. */
		InventorySlot* findWeapon(World& world, ChunkId chunk_id, PointId point_id)
		{
			InventoryComponent* inventory = findInventory(world, chunk_id, point_id);
			if(inventory == nullptr) return nullptr;
			for(InventorySlot& item : inventory->slots)
				if(!item.isEmpty() && getItemDefinition(item.item_definition_id).category_id == item_category_ids::weapon)
					return &item;
			return nullptr;
		}

		/** @brief Estimates hit chance from target angular size and the two independent spread sources. */
		float estimateHitProbability(float target_height, double distance, float spread_degrees)
		{
			if(distance <= 0.001) return 1.0f;
			const float target_half_angle = static_cast<float>(std::atan2(target_height * 0.14f, distance));
			const float spread_radians = glm::radians(glm::max(spread_degrees, 0.05f));
			const float ratio = target_half_angle / spread_radians;
			return glm::clamp(ratio * ratio, 0.02f, 1.0f);
		}

		/** @brief Returns expected weapon damage per second, or zero when no firearm is available. */
		float estimateWeaponDamagePerSecond(World& world, ChunkId chunk_id, PointId point_id)
		{
			InventorySlot* weapon = findWeapon(world, chunk_id, point_id);
			if(weapon == nullptr) return 0.0f;
			const WeaponItemParameters& parameters = getWeaponItemParameters(weapon->item_definition_id);
			return glm::max(0.0f, parameters.damage) * glm::max(0.0f, parameters.rounds_per_minute) / 60.0f;
		}

		/** @brief Estimates damage per second after accounting for this shooter and target size. */
		float estimateExpectedDamagePerSecond(World& world, ChunkId chunk_id, PointId point_id,
			const HumanComponent& human, float target_height, double distance)
		{
			InventorySlot* weapon = findWeapon(world, chunk_id, point_id);
			if(weapon == nullptr) return 1.0f;
			const WeaponItemParameters& parameters = getWeaponItemParameters(weapon->item_definition_id);
			const float hit_probability = estimateHitProbability(target_height, distance,
				parameters.spread_degrees + human.getSpreadDegrees());
			return glm::max(0.1f,
				parameters.damage * parameters.rounds_per_minute / 60.0f * hit_probability);
		}

		/** @brief Counts current-tick allies already allocated to one target. */
		const TargetAllocation* findAllocation(const DynamicArray<TargetAllocation>& allocations,
			const UnitSighting& sighting)
		{
			for(const TargetAllocation& allocation : allocations)
				if(allocation.target_slot_id == sighting.target_slot_id
					&& allocation.target_generation_id == sighting.target_generation_id) return &allocation;
			return nullptr;
		}

		/** @brief Scores a visible or remembered hostile using hit chance, threat, urgency, and team focus. */
		float scoreTarget(World& world, const UnitSighting& sighting, const PointComponentSlot& own_slot,
			const HumanComponent& human, const NpcTarget& previous_target, const glm::dvec3& own_position,
			const glm::dvec3& target_position, const WorldBody& body,
			const DynamicArray<TargetAllocation>& allocations)
		{
			const PointComponentSlot& target_slot = world.data.getPointComponentSlot(sighting.target_slot_id);
			const HumanComponent& target = *static_cast<const HumanComponent*>(target_slot.getData());
			const glm::dvec3 offset = target_position - own_position;
			const double distance = glm::length(offset);
			const float expected_damage = estimateExpectedDamagePerSecond(world,
				own_slot.chunk_id, own_slot.point_id, human, target.getHeight(), distance);

			const float target_threat_dps = estimateWeaponDamagePerSecond(world,
				target_slot.chunk_id, target_slot.point_id);
			const float threat_hit_probability = estimateHitProbability(human.getHeight(), distance,
				target.getSpreadDegrees() + 1.5f);
			float score = expected_damage * (1.0f + 0.75f * glm::clamp(
				target_threat_dps * threat_hit_probability / 20.0f, 0.0f, 1.0f));

			const DamageableComponent* health = findDamageable(world, target_slot.chunk_id, target_slot.point_id);
			if(health != nullptr)
			{
				const float maximum_health = getDamageableDefinition(health->definition_id).max_health;
				if(maximum_health > 0.0f)
					score *= 1.0f + 0.35f * (1.0f - glm::clamp(health->getHealth() / maximum_health, 0.0f, 1.0f));
			}
			if(sighting.visible_this_tick) score *= 1.35f;
			if(sighting.target_slot_id == previous_target.slot_id
				&& sighting.target_generation_id == previous_target.generation_id) score *= 1.4f;

			const uint32 now = danikk_engine::getMsTimestamp();
			const uint32 attack_age = now - sighting.last_attack_timestamp_ms;
			if(sighting.last_attack_timestamp_ms != 0 && attack_age <= 3000)
			{
				if(sighting.last_attacked_point_id == own_slot.point_id)
				{
					const glm::dvec3 toward_attacker = distance > 0.001 ? offset / distance : glm::dvec3(0.0);
					const glm::dvec3 own_forward = body.rotation * glm::dvec3(human.getForward());
					const double facing_dot = glm::dot(glm::normalize(own_forward), toward_attacker);
					// A recent hit from the side or rear can outweigh the previous focus target.
					score *= std::abs(facing_dot) < 0.55 ? 5.0f : 3.5f;
				}
				else score *= 1.5f;
			}

			const TargetAllocation* allocation = findAllocation(allocations, sighting);
			if(allocation != nullptr)
			{
				const float saturation = static_cast<float>(allocation->unit_count) * 0.8f
					+ allocation->expected_damage_per_second / glm::max(expected_damage, 1.0f) * 0.2f;
				score /= 1.0f + saturation;
			}
			return score / (1.0f + static_cast<float>(distance) * 0.015f);
		}

		/** @brief Adds one unit's planned firepower to the current target allocation. */
		void recordAllocation(DynamicArray<TargetAllocation>& allocations,
			const UnitSighting& sighting, float expected_damage_per_second)
		{
			for(TargetAllocation& allocation : allocations)
				if(allocation.target_slot_id == sighting.target_slot_id
					&& allocation.target_generation_id == sighting.target_generation_id)
				{
					++allocation.unit_count;
					allocation.expected_damage_per_second += expected_damage_per_second;
					return;
				}
			TargetAllocation allocation;
			allocation.target_slot_id = sighting.target_slot_id;
			allocation.target_generation_id = sighting.target_generation_id;
			allocation.unit_count = 1;
			allocation.expected_damage_per_second = expected_damage_per_second;
			allocations.push(allocation);
		}
	}

	/** @brief Finds the inventory component on an NPC's point, if one is attached. */
	InventoryComponent* findInventory(World& world, ChunkId chunk_id, PointId point_id)
	{
		for(PointComponentSlotId id : world.data.getChunk(chunk_id).point_component_slot_ids)
		{
			if(!world.data.isPointComponentSlotActive(id)) continue;
			PointComponentSlot& slot = world.data.getPointComponentSlot(id);
			if(slot.point_id == point_id && slot.type_id == PointComponentTypeId::inventory)
				return static_cast<InventoryComponent*>(slot.getData());
		}
		return nullptr;
	}

	/** @brief Fills one unit's attack, movement, look, and equipment commands from faction intel. */
	void assignOrder(World& world, UnitSystem& system, PointComponentSlot& controller_slot,
		NpcControllerComponent& controller, HumanComponent& human,
		DynamicArray<TargetAllocation>& allocations)
	{
		DynamicArray<UnitSighting>& known_sightings = system.known_sightings;
		const NpcTarget previous_target{controller.order.target_slot_id,
			controller.order.target_point_id, controller.order.target_generation_id};
		controller.order.clear();
		const glm::dvec3 own_position = world.getPointWorldPosition(controller_slot.chunk_id, controller_slot.point_id);
		const WorldBodyId own_body_id = world.data.getChunk(controller_slot.chunk_id).world_body_id;
		const WorldBody& body = world.data.getWorldBody(own_body_id);
		const UnitSighting* best = nullptr;
		float best_score = -1.0f;
		for(const UnitSighting& sighting : known_sightings)
		{
			if(sighting.faction_id != human.faction_id || !isRecentSighting(sighting)
				|| !isValidSighting(world, sighting)) continue;
			const PointComponentSlot& sighted_slot = world.data.getPointComponentSlot(sighting.target_slot_id);
			if(world.data.getChunk(sighted_slot.chunk_id).world_body_id != own_body_id) continue;
			const glm::dvec3 target_position = world.getPointWorldPosition(sighted_slot.chunk_id, sighted_slot.point_id);
			const float score = scoreTarget(world, sighting, controller_slot, human, previous_target,
				own_position, target_position, body, allocations);
			if(score > best_score)
			{
				best_score = score;
				best = &sighting;
			}
		}
		if(best == nullptr) return;

		controller.order.has_attack_target = true;
		controller.order.target_slot_id = best->target_slot_id;
		controller.order.target_point_id = best->target_point_id;
		controller.order.target_generation_id = best->target_generation_id;
		const PointComponentSlot& target_slot = world.data.getPointComponentSlot(best->target_slot_id);
		const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, target_slot.point_id);
		const glm::dvec3 shot_origin = own_position
			+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.2f));
		const glm::dvec3 delta = target_position - shot_origin;
		const double distance = glm::length(delta);
		if(distance <= 0.001) return;
		InventorySlot* weapon = findWeapon(world, controller_slot.chunk_id, controller_slot.point_id);
		if(weapon != nullptr && !weapon->is_equipped)
		{
			controller.order.has_equip_order = true;
			controller.order.equip_item_definition_id = weapon->item_definition_id;
		}
		const RaycastHit hit = world.raycast(shot_origin, vec3(delta), static_cast<float>(distance));
		const bool visible = !hit.hit || hit.distance >= distance - 0.1;
		if(visible)
		{
			controller.order.has_look_target = true;
			controller.order.look_target_world = target_position;
			rememberSighting(world, system, human.faction_id,
				NpcTarget{best->target_slot_id, best->target_point_id, best->target_generation_id},
				target_position);

			if(weapon != nullptr && weapon->is_equipped)
			{
				const vec3 local_direction = vec3(glm::conjugate(body.rotation) * delta);
				const float direction_length = glm::length(local_direction);
				if(direction_length > 0.001f
					&& glm::dot(human.getWeaponDirection(), local_direction / direction_length)
						>= std::cos(glm::radians(10.0f)))
				{
					controller.order.should_fire = true;
					controller.order.fire_item_definition_id = weapon->item_definition_id;
				}
			}
		}
		else
		{
			controller.order.has_move_target = true;
			controller.order.move_target_world = best->position;
			controller.order.has_look_target = true;
			controller.order.look_target_world = best->position;
		}
		const HumanComponent& target_human = *static_cast<const HumanComponent*>(target_slot.getData());
		const float assigned_damage = estimateExpectedDamagePerSecond(world, controller_slot.chunk_id,
			controller_slot.point_id, human, target_human.getHeight(), distance);
		recordAllocation(allocations, *best, assigned_damage);
	}

	/** @brief Resolves an active NPC controller and its paired human component. */
	bool getActiveNpc(World& world, PointComponentSlotId controller_id,
		PointComponentSlot*& controller_slot, NpcControllerComponent*& controller, HumanComponent*& human)
	{
		if(!world.data.isPointComponentSlotActive(controller_id)) return false;
		controller_slot = &world.data.getPointComponentSlot(controller_id);
		if(controller_slot->type_id != PointComponentTypeId::npc_controller
			|| !world.data.isPointActive(controller_slot->point_id)
			|| !world.data.isChunkActive(controller_slot->chunk_id)) return false;
		controller = static_cast<NpcControllerComponent*>(controller_slot->getData());
		if(!controller->enabled)
		{
			controller->order.clear();
			return false;
		}
		if(!world.data.isPointComponentSlotActive(controller->human_slot_id))
		{
			controller->order.clear();
			return false;
		}
		PointComponentSlot& human_slot = world.data.getPointComponentSlot(controller->human_slot_id);
		if(human_slot.type_id != PointComponentTypeId::human
			|| human_slot.point_id != controller_slot->point_id
			|| human_slot.chunk_id != controller_slot->chunk_id)
		{
			controller->order.clear();
			return false;
		}
		human = static_cast<HumanComponent*>(human_slot.getData());
		const DamageableComponent* health = findDamageable(world, controller_slot->chunk_id, controller_slot->point_id);
		if(health == nullptr || health->getHealth() <= 0.0f)
		{
			controller->order.clear();
			return false;
		}
		return true;
	}

	/** @brief Assigns commands to every active NPC using the latest faction intelligence. */
	void assignOrders(World& world, UnitSystem& system,
		const DynamicArray<PointComponentSlotId>& slot_ids)
	{
		DSE_WATCH();
		DynamicArray<TargetAllocation> allocations;
		for(PointComponentSlotId id : slot_ids)
		{
			PointComponentSlot* controller_slot = nullptr;
			NpcControllerComponent* controller = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveNpc(world, id, controller_slot, controller, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			assignOrder(world, system, *controller_slot, *controller, *human, allocations);
		}
	}

	/** @brief Resolves a body's current world-space position for a stored local search point. */
	glm::dvec3 getSearchWorldPosition(const World& world, const UnitSearchLocation& location)
	{
		if(!world.data.isWorldBodyActive(location.body_id)) return glm::dvec3(std::numeric_limits<double>::max());
		const WorldBody& body = world.data.getWorldBody(location.body_id);
		return body.position + body.rotation * location.body_local_position;
	}

		/** @brief Checks corpses in one nearby chunk and defers destruction of visible eligible bodies. */
		void inspectChunkCorpses(World& world, const UnitSystem& system,
			const glm::dvec3& search_position,
			const glm::dvec3& eye_position, double search_radius_squared, ChunkId chunk_id)
	{
			if(!world.data.isChunkActive(chunk_id)) return;
			const Chunk& chunk = world.data.getChunk(chunk_id);
			for(PointComponentSlotId id : chunk.getHumanComponentSlotIds())
		{
			if(!world.data.isPointComponentSlotActive(id)) continue;
			const PointComponentSlot& corpse_slot = world.data.getPointComponentSlot(id);
				if(!world.data.isPointActive(corpse_slot.point_id)) continue;
			const HumanComponent& corpse = *static_cast<const HumanComponent*>(corpse_slot.getData());
				if(!corpse.hasDeathTimestamp()) continue;
			if(corpse.faction_id >= world.data.factions.size()
				|| system.faction_id >= world.data.factions.size()) continue;
			const bool is_ally = corpse.faction_id == system.faction_id;
			const bool is_hostile = world.data.factions.getRelation(
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
			const WorldBody& body = world.data.getWorldBody(location.body_id);
			const PlanetComponent* planet = nullptr;
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(id)) continue;
				const WorldBodyComponentSlot& component = world.data.getWorldBodyComponentSlot(id);
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
							|| !world.data.isChunkActive(chunk_id)) continue;
						const Chunk& chunk = world.data.getChunk(chunk_id);
						if(chunk.world_body_id != location.body_id || chunk.grid_coordinate != coordinate) continue;
						inspectChunkCorpses(world, system, search_position, eye_position, search_radius_squared, chunk_id);
					}
		}

	/** @brief Orders available units to inspect lost-contact locations after ten seconds without sightings. */
	void assignSearchOrders(World& world, UnitSystem& system,
		const DynamicArray<PointComponentSlotId>& slot_ids)
	{
		DSE_WATCH();
		if(!system.has_had_enemy_contact || system.enemy_contact_this_tick
			|| system.time_since_enemy_visible < 10.0f || system.search_locations.size() == 0) return;

		DynamicArray<UnitSearchCandidate>& candidates = system.search_candidates;
		candidates.clear();
		candidates.reserve(slot_ids.size());
		for(PointComponentSlotId id : slot_ids)
		{
			PointComponentSlot* slot = nullptr;
			NpcControllerComponent* controller = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveNpc(world, id, slot, controller, human) || human->faction_id != system.faction_id) continue;
			const WorldBodyId body_id = world.data.getChunk(slot->chunk_id).world_body_id;
			const glm::dvec3 position = world.getPointWorldPosition(slot->chunk_id, slot->point_id);
			candidates.push(UnitSearchCandidate{id, slot, controller, human, body_id, position, false});
		}

		// Reserve controllers already assigned to valid locations before assigning the remaining units.
		for(const UnitSearchLocation& location : system.search_locations)
		{
			if(location.assigned_controller_slot_id == static_cast<PointComponentSlotId>(-1)) continue;
			for(UnitSearchCandidate& candidate : candidates)
			{
				if(candidate.slot_id != location.assigned_controller_slot_id
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
			if(location.assigned_controller_slot_id != static_cast<PointComponentSlotId>(-1))
			{
				for(UnitSearchCandidate& candidate : candidates)
					if(candidate.slot_id == location.assigned_controller_slot_id)
					{
						assigned_unit = &candidate;
						break;
					}
				if(assigned_unit == nullptr || assigned_unit->body_id != location.body_id)
				{
					if(assigned_unit != nullptr) assigned_unit->assigned_to_search = false;
					location.assigned_controller_slot_id = static_cast<PointComponentSlotId>(-1);
				}
				else if(!assigned_unit->controller->order.has_attack_target
					&& world.data.isWorldBodyActive(location.body_id))
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
					assigned_unit->controller->order.has_move_target = true;
					assigned_unit->controller->order.move_target_world = target_position;
					assigned_unit->controller->order.has_look_target = true;
					assigned_unit->controller->order.look_target_world = target_position;
					++index;
					continue;
				}
			}

			if(location.assigned_controller_slot_id == static_cast<PointComponentSlotId>(-1))
			{
				const glm::dvec3 target_position = getSearchWorldPosition(world, location);
				UnitSearchCandidate* nearest_candidate = nullptr;
				double nearest_distance_squared = std::numeric_limits<double>::max();
				for(UnitSearchCandidate& candidate : candidates)
				{
					if(candidate.body_id != location.body_id || candidate.controller->order.has_attack_target
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
				location.assigned_controller_slot_id = nearest_candidate->slot_id;
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
				nearest_candidate->controller->order.has_move_target = true;
				nearest_candidate->controller->order.move_target_world = target_position;
				nearest_candidate->controller->order.has_look_target = true;
				nearest_candidate->controller->order.look_target_world = target_position;
			}
			++index;
		}
		candidates.clear();
	}

	/** @brief Removes one stored post-combat assignment while preserving the remaining order. */
	void removePostCombatAssignment(DynamicArray<UnitPostCombatAssignment>& assignments, size_t index)
	{
		for(size_t i = index + 1; i < assignments.size(); ++i)
			assignments[i - 1] = static_cast<UnitPostCombatAssignment&&>(assignments[i]);
		assignments.resize(assignments.size() - 1);
	}

	/** @brief Assigns stable dispersal positions and staggered look-around orders outside combat. */
	void assignPostCombatOrders(World& world, UnitSystem& system,
		const DynamicArray<PointComponentSlotId>& slot_ids)
	{
		DSE_WATCH();
		DynamicArray<UnitPostCombatAssignment>& assignments = system.post_combat_assignments;
		if(!system.has_had_enemy_contact || system.enemy_contact_this_tick)
		{
			assignments.clear();
			return;
		}

		DynamicArray<UnitSearchCandidate>& candidates = system.search_candidates;
		candidates.clear();
		candidates.reserve(slot_ids.size());
		for(PointComponentSlotId id : slot_ids)
		{
			PointComponentSlot* slot = nullptr;
			NpcControllerComponent* controller = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveNpc(world, id, slot, controller, human) || human->faction_id != system.faction_id) continue;
			const WorldBodyId body_id = world.data.getChunk(slot->chunk_id).world_body_id;
			const glm::dvec3 position = world.getPointWorldPosition(slot->chunk_id, slot->point_id);
			candidates.push(UnitSearchCandidate{id, slot, controller, human, body_id, position, false});
		}

		// Drop assignments for removed units, units that moved to another body, and units now fighting/searching.
		for(size_t i = 0; i < assignments.size();)
		{
			UnitPostCombatAssignment& assignment = assignments[i];
			UnitSearchCandidate* candidate = nullptr;
			for(UnitSearchCandidate& current : candidates)
				if(current.slot_id == assignment.controller_slot_id)
				{
					candidate = &current;
					break;
				}
			if(candidate == nullptr || candidate->slot->generation_id != assignment.controller_generation_id
				|| candidate->body_id != assignment.body_id || candidate->controller->order.has_attack_target
				|| candidate->controller->order.has_move_target)
			{
				removePostCombatAssignment(assignments, i);
				continue;
			}
			++i;
		}

		const uint32 now = danikk_engine::getMsTimestamp();
		for(UnitSearchCandidate& candidate : candidates)
		{
			NpcControllerComponent& controller = *candidate.controller;
			HumanComponent& human = *candidate.human;
			// Search orders have priority; combat orders are already assigned by assignOrders().
			if(controller.order.has_attack_target || controller.order.has_move_target) continue;

			UnitPostCombatAssignment* assignment = nullptr;
			for(UnitPostCombatAssignment& existing : assignments)
				if(existing.controller_slot_id == candidate.slot_id
					&& existing.controller_generation_id == candidate.slot->generation_id
					&& existing.body_id == candidate.body_id)
				{
					assignment = &existing;
					break;
				}

			const Chunk& chunk = world.data.getChunk(candidate.slot->chunk_id);
			const WorldBody& body = world.data.getWorldBody(candidate.body_id);
			const glm::dvec3 body_local_position = chunk.position
				+ glm::dvec3(world.data.getPoint(candidate.slot->point_id).position);
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
				created.controller_slot_id = candidate.slot_id;
				created.controller_generation_id = candidate.slot->generation_id;
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
				controller.order.has_move_target = true;
				controller.order.move_target_world = move_target;
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
			controller.order.has_look_target = true;
			controller.order.look_target_world = body.position
				+ body.rotation * (body_local_position + glm::dvec3(look_direction * 8.0f));
		}
	}
}
