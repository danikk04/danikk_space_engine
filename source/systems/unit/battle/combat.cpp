#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>
#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/item/definition.h>

#include <danikk_space_engine/systems/unit/internal/battle.h>
#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/perception.h>
#include <danikk_space_engine/systems/unit/internal/state.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		/** @brief Current-tick assignments used to spread a faction's fire across valid targets. */
		struct TargetAllocation
		{
			HumanId target_human_id = static_cast<HumanId>(-1);
			uint64 target_generation_id = 0;
			uint32 unit_count = 0;
			float expected_damage_per_second = 0.0f;
		};

		/** @brief Finds the first weapon stack in an NPC's inventory. */
		InventorySlot* findWeapon(HumanData& human)
		{
			for(InventorySlot& item : human.inventory.slots)
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
		float estimateWeaponDamagePerSecond(HumanData& human)
		{
			InventorySlot* weapon = findWeapon(human);
			if(weapon == nullptr) return 0.0f;
			const WeaponItemParameters& parameters = getWeaponItemParameters(weapon->item_definition_id);
			return glm::max(0.0f, parameters.damage) * glm::max(0.0f, parameters.rounds_per_minute) / 60.0f;
		}

		/** @brief Estimates damage per second after accounting for this shooter and target size. */
		float estimateExpectedDamagePerSecond(HumanData& owner,
			const HumanState& human, float target_height, double distance)
		{
			InventorySlot* weapon = findWeapon(owner);
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
				if(allocation.target_human_id == sighting.target_human_id
					&& allocation.target_generation_id == sighting.target_generation_id) return &allocation;
			return nullptr;
		}

		/** @brief Scores a visible or remembered hostile using hit chance, threat, urgency, and team focus. */
		float scoreTarget(World& world, const UnitSighting& sighting, HumanData& own_data,
			const HumanState& human, const UnitTarget& previous_target, const glm::dvec3& own_position,
			const glm::dvec3& target_position, const WorldBody& body,
			const DynamicArray<TargetAllocation>& allocations)
		{
			const HumanData* target_data = world.getHumanSystem().findHuman(sighting.target_human_id);
			if(target_data == nullptr) return 0.0f;
			const HumanState& target = *target_data;
			const glm::dvec3 offset = target_position - own_position;
			const double distance = glm::length(offset);
			const float expected_damage = estimateExpectedDamagePerSecond(
				own_data, human, target.getHeight(), distance);

			const float target_threat_dps = estimateWeaponDamagePerSecond(*const_cast<HumanData*>(target_data));
			const float threat_hit_probability = estimateHitProbability(human.getHeight(), distance,
				target.getSpreadDegrees() + 1.5f);
			float score = expected_damage * (1.0f + 0.75f * glm::clamp(
				target_threat_dps * threat_hit_probability / 20.0f, 0.0f, 1.0f));

			const DamageableComponent* health = &target_data->damageable;
			if(health != nullptr)
			{
				const float maximum_health = getDamageableDefinition(health->definition_id).max_health;
				if(maximum_health > 0.0f)
					score *= 1.0f + 0.35f * (1.0f - glm::clamp(health->getHealth() / maximum_health, 0.0f, 1.0f));
			}
			if(sighting.visible_this_tick) score *= 1.35f;
			if(sighting.target_human_id == previous_target.human_id
				&& sighting.target_generation_id == previous_target.generation_id) score *= 1.4f;

			const uint32 now = danikk_engine::getMsTimestamp();
			const uint32 attack_age = now - sighting.last_attack_timestamp_ms;
			if(sighting.last_attack_timestamp_ms != 0 && attack_age <= 3000)
			{
				if(sighting.last_attacked_human_id == own_data.id)
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
				if(allocation.target_human_id == sighting.target_human_id
					&& allocation.target_generation_id == sighting.target_generation_id)
				{
					++allocation.unit_count;
					allocation.expected_damage_per_second += expected_damage_per_second;
					return;
				}
			TargetAllocation allocation;
			allocation.target_human_id = sighting.target_human_id;
			allocation.target_generation_id = sighting.target_generation_id;
			allocation.unit_count = 1;
			allocation.expected_damage_per_second = expected_damage_per_second;
			allocations.push(allocation);
		}
	}

	/** @brief Finds the inventory component on an NPC's point, if one is attached. */
	/** @brief Fills one unit's attack, movement, look, and equipment commands from faction intel. */
	void assignOrder(World& world, UnitGroup& system, HumanData& human_data,
		UnitAgentState& agent, HumanState& human,
		DynamicArray<TargetAllocation>& allocations)
	{
		DynamicArray<UnitSighting>& known_sightings = system.known_sightings;
		const UnitTarget previous_target{agent.order.target_human_id,
			agent.order.target_generation_id};
		agent.order.clear();
		const glm::dvec3 own_position = world.getHumanSystem().getWorldPosition(human_data);
		const WorldBodyId own_body_id = human_data.world_body_id;
		const WorldBody& body = world.getWorldBodySystem().getWorldBody(own_body_id);
		const UnitSighting* best = nullptr;
		float best_score = -1.0f;
		for(const UnitSighting& sighting : known_sightings)
		{
			if(sighting.faction_id != human.faction_id || !isRecentSighting(sighting)
				|| !isValidSighting(world, sighting)) continue;
			const HumanData* sighted = world.getHumanSystem().findHuman(sighting.target_human_id);
			if(sighted == nullptr || sighted->world_body_id != own_body_id) continue;
			const glm::dvec3 target_position = world.getHumanSystem().getWorldPosition(*sighted);
			const float score = scoreTarget(world, sighting, human_data, human, previous_target,
				own_position, target_position, body, allocations);
			if(score > best_score)
			{
				best_score = score;
				best = &sighting;
			}
		}
		if(best == nullptr) return;

		agent.order.has_attack_target = true;
		agent.order.target_human_id = best->target_human_id;
		agent.order.target_generation_id = best->target_generation_id;
		HumanData* target_data = world.getHumanSystem().findHuman(best->target_human_id);
		if(target_data == nullptr) return;
		const glm::dvec3 target_position = world.getHumanSystem().getWorldPosition(*target_data);
		const glm::dvec3 shot_origin = own_position
			+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.2f));
		const glm::dvec3 delta = target_position - shot_origin;
		const double distance = glm::length(delta);
		if(distance <= 0.001) return;
			InventorySlot* weapon = findWeapon(human_data);
		if(weapon != nullptr && !weapon->is_equipped)
		{
			agent.order.has_equip_order = true;
			agent.order.equip_item_definition_id = weapon->item_definition_id;
		}
		const RaycastHit hit = world.getRaycastSystem().cast(shot_origin, vec3(delta), static_cast<float>(distance));
		const bool visible = !hit.hit || hit.distance >= distance - 0.1;
		if(visible)
		{
			agent.substate = UnitSubstate::attack;
			agent.order.has_look_target = true;
			agent.order.look_target_world = target_position;
			rememberSighting(world, system, human.faction_id,
				UnitTarget{best->target_human_id, best->target_generation_id},
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
		const HumanState& target_human = *target_data;
		const float assigned_damage = estimateExpectedDamagePerSecond(human_data,
			human, target_human.getHeight(), distance);
		recordAllocation(allocations, *best, assigned_damage);
	}

	/** @brief Assigns commands to every active NPC using the latest faction intelligence. */
	void assignOrders(World& world, UnitGroup& system)
	{
		DSE_WATCH();
		DynamicArray<TargetAllocation> allocations;
		for(UnitAgentState& agent : system.agents)
		{
			HumanData* human = nullptr;
			if(!getActiveUnit(world, agent, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			assignOrder(world, system, *human, agent, *human, allocations);
		}
	}
}
