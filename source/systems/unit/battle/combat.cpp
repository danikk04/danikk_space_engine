#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/systems/planet/planet.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/item/definition.h>

#include "../internal.h"

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
			const HumanComponent& human, const UnitTarget& previous_target, const glm::dvec3& own_position,
			const glm::dvec3& target_position, const WorldBody& body,
			const DynamicArray<TargetAllocation>& allocations)
		{
			const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(sighting.target_slot_id);
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
		for(PointComponentSlotId id : world.chunks.getChunk(chunk_id).point_component_slot_ids)
		{
			if(!world.point_data.isPointComponentSlotActive(id)) continue;
			PointComponentSlot& slot = world.point_data.getPointComponentSlot(id);
			if(slot.point_id == point_id && slot.type_id == PointComponentTypeId::inventory)
				return static_cast<InventoryComponent*>(slot.getData());
		}
		return nullptr;
	}

	/** @brief Fills one unit's attack, movement, look, and equipment commands from faction intel. */
	void assignOrder(World& world, UnitGroup& system, PointComponentSlot& human_slot,
		UnitAgentState& agent, HumanComponent& human,
		DynamicArray<TargetAllocation>& allocations)
	{
		DynamicArray<UnitSighting>& known_sightings = system.known_sightings;
		const UnitTarget previous_target{agent.order.target_slot_id,
			agent.order.target_point_id, agent.order.target_generation_id};
		agent.order.clear();
		const glm::dvec3 own_position = world.getPointWorldPosition(human_slot.chunk_id, human_slot.point_id);
		const WorldBodyId own_body_id = world.chunks.getChunk(human_slot.chunk_id).world_body_id;
		const WorldBody& body = world.world_body_data.getWorldBody(own_body_id);
		const UnitSighting* best = nullptr;
		float best_score = -1.0f;
		for(const UnitSighting& sighting : known_sightings)
		{
			if(sighting.faction_id != human.faction_id || !isRecentSighting(sighting)
				|| !isValidSighting(world, sighting)) continue;
			const PointComponentSlot& sighted_slot = world.point_data.getPointComponentSlot(sighting.target_slot_id);
			if(world.chunks.getChunk(sighted_slot.chunk_id).world_body_id != own_body_id) continue;
			const glm::dvec3 target_position = world.getPointWorldPosition(sighted_slot.chunk_id, sighted_slot.point_id);
			const float score = scoreTarget(world, sighting, human_slot, human, previous_target,
				own_position, target_position, body, allocations);
			if(score > best_score)
			{
				best_score = score;
				best = &sighting;
			}
		}
		if(best == nullptr) return;

		agent.order.has_attack_target = true;
		agent.order.target_slot_id = best->target_slot_id;
		agent.order.target_point_id = best->target_point_id;
		agent.order.target_generation_id = best->target_generation_id;
		const PointComponentSlot& target_slot = world.point_data.getPointComponentSlot(best->target_slot_id);
		const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, target_slot.point_id);
		const glm::dvec3 shot_origin = own_position
			+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.2f));
		const glm::dvec3 delta = target_position - shot_origin;
		const double distance = glm::length(delta);
		if(distance <= 0.001) return;
		InventorySlot* weapon = findWeapon(world, human_slot.chunk_id, human_slot.point_id);
		if(weapon != nullptr && !weapon->is_equipped)
		{
			agent.order.has_equip_order = true;
			agent.order.equip_item_definition_id = weapon->item_definition_id;
		}
		const RaycastHit hit = world.raycasts.cast(shot_origin, vec3(delta), static_cast<float>(distance));
		const bool visible = !hit.hit || hit.distance >= distance - 0.1;
		if(visible)
		{
			agent.substate = UnitSubstate::attack;
			agent.order.has_look_target = true;
			agent.order.look_target_world = target_position;
			rememberSighting(world, system, human.faction_id,
				UnitTarget{best->target_slot_id, best->target_point_id, best->target_generation_id},
				target_position);

			if(weapon != nullptr && weapon->is_equipped)
			{
				const vec3 local_direction = vec3(glm::conjugate(body.rotation) * delta);
				const float direction_length = glm::length(local_direction);
				if(direction_length > 0.001f
					&& glm::dot(human.getWeaponDirection(), local_direction / direction_length)
						>= std::cos(glm::radians(10.0f)))
				{
					agent.order.should_fire = true;
					agent.order.fire_item_definition_id = weapon->item_definition_id;
				}
			}
		}
		else
		{
			agent.substate = UnitSubstate::pursuit;
			agent.order.has_move_target = true;
			agent.order.move_target_world = best->position;
			agent.order.has_look_target = true;
			agent.order.look_target_world = best->position;
		}
		const HumanComponent& target_human = *static_cast<const HumanComponent*>(target_slot.getData());
		const float assigned_damage = estimateExpectedDamagePerSecond(world, human_slot.chunk_id,
			human_slot.point_id, human, target_human.getHeight(), distance);
		recordAllocation(allocations, *best, assigned_damage);
	}

	/** @brief Resolves a registered live human and its point slot. */
	bool getActiveUnit(World& world, UnitAgentState& agent,
		PointComponentSlot*& human_slot, HumanComponent*& human)
	{
		if(!agent.enabled || !world.point_data.isPointComponentSlotActive(agent.human_slot_id)) return false;
		human_slot = &world.point_data.getPointComponentSlot(agent.human_slot_id);
		if(human_slot->type_id != PointComponentTypeId::human
			|| human_slot->generation_id != agent.human_generation_id
			|| !world.point_data.isPointActive(human_slot->point_id)
			|| !world.chunks.isChunkActive(human_slot->chunk_id)) return false;
		human = static_cast<HumanComponent*>(human_slot->getData());
		const DamageableComponent* health = findDamageable(world, human_slot->chunk_id, human_slot->point_id);
		if(health == nullptr || health->getHealth() <= 0.0f)
		{
			agent.order.clear();
			return false;
		}
		return true;
	}

	/** @brief Assigns commands to every active NPC using the latest faction intelligence. */
	void assignOrders(World& world, UnitGroup& system)
	{
		DSE_WATCH();
		DynamicArray<TargetAllocation> allocations;
		for(UnitAgentState& agent : system.agents)
		{
			PointComponentSlot* human_slot = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveUnit(world, agent, human_slot, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			assignOrder(world, system, *human_slot, agent, *human, allocations);
		}
	}
}
