#include <cmath>
#include <limits>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
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
		NpcControllerComponent& controller, HumanComponent& human)
	{
		DynamicArray<UnitSighting>& known_sightings = system.known_sightings;
		controller.order.clear();
		const glm::dvec3 own_position = world.getPointWorldPosition(controller_slot.chunk_id, controller_slot.point_id);
		const WorldBodyId own_body_id = world.data.getChunk(controller_slot.chunk_id).world_body_id;
		const UnitSighting* closest = nullptr;
		double closest_distance_squared = std::numeric_limits<double>::max();
		for(const UnitSighting& sighting : known_sightings)
		{
			if(sighting.faction_id != human.faction_id || !isRecentSighting(sighting)
				|| !isValidSighting(world, sighting)) continue;
			const PointComponentSlot& sighted_slot = world.data.getPointComponentSlot(sighting.target_slot_id);
			if(world.data.getChunk(sighted_slot.chunk_id).world_body_id != own_body_id) continue;
			const glm::dvec3 offset = sighting.position - own_position;
			const double distance_squared = glm::dot(offset, offset);
			if(distance_squared < closest_distance_squared)
			{
				closest_distance_squared = distance_squared;
				closest = &sighting;
			}
		}
		if(closest == nullptr) return;

		controller.order.has_attack_target = true;
		controller.order.target_slot_id = closest->target_slot_id;
		controller.order.target_point_id = closest->target_point_id;
		controller.order.target_generation_id = closest->target_generation_id;
		const PointComponentSlot& target_slot = world.data.getPointComponentSlot(closest->target_slot_id);
		const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, target_slot.point_id);
		const Chunk& chunk = world.data.getChunk(controller_slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dvec3 shot_origin = own_position
			+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.2f));
		const glm::dvec3 delta = target_position - shot_origin;
		const double distance = glm::length(delta);
		if(distance <= 0.001) return;
		InventoryComponent* inventory = findInventory(world, controller_slot.chunk_id, controller_slot.point_id);
		InventorySlot* weapon = nullptr;
		if(inventory != nullptr)
		{
			for(InventorySlot& item : inventory->slots)
			{
				if(item.isEmpty() || getItemDefinition(item.item_definition_id).type_id != item_type_ids::weapon) continue;
				weapon = &item;
				break;
			}
		}
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
				NpcTarget{closest->target_slot_id, closest->target_point_id, closest->target_generation_id},
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
			controller.order.move_target_world = closest->position;
			controller.order.has_look_target = true;
			controller.order.look_target_world = closest->position;
		}
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
		for(PointComponentSlotId id : slot_ids)
		{
			PointComponentSlot* controller_slot = nullptr;
			NpcControllerComponent* controller = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveNpc(world, id, controller_slot, controller, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			assignOrder(world, system, *controller_slot, *controller, *human);
		}
	}

	/** @brief Resolves a body's current world-space position for a stored local search point. */
	glm::dvec3 getSearchWorldPosition(const World& world, const UnitSearchLocation& location)
	{
		if(!world.data.isWorldBodyActive(location.body_id)) return glm::dvec3(std::numeric_limits<double>::max());
		const WorldBody& body = world.data.getWorldBody(location.body_id);
		return body.position + body.rotation * location.body_local_position;
	}

	/** @brief Deletes visible allied or hostile corpses close to an inspected search location. */
	void removeCorpsesAtSearchLocation(World& world, const UnitSystem& system,
		const PointComponentSlot& inspector_slot, const HumanComponent& inspector,
		const UnitSearchLocation& location, const glm::dvec3& search_position)
	{
		const glm::dvec3 eye_position = world.getPointWorldPosition(inspector_slot.chunk_id, inspector_slot.point_id)
			+ glm::dvec3(inspector.up * (inspector.getHeight() * 0.25f));
		const double search_radius_squared = 2.5 * 2.5;
		for(PointComponentSlotId id : world.data.getUsedPointComponentSlotIds())
		{
			if(!world.data.isPointComponentSlotActive(id)) continue;
			const PointComponentSlot& corpse_slot = world.data.getPointComponentSlot(id);
			if(corpse_slot.type_id != PointComponentTypeId::human || !world.data.isChunkActive(corpse_slot.chunk_id)
				|| !world.data.isPointActive(corpse_slot.point_id)) continue;
			const Chunk& corpse_chunk = world.data.getChunk(corpse_slot.chunk_id);
			if(corpse_chunk.world_body_id != location.body_id) continue;
			const HumanComponent& corpse = *static_cast<const HumanComponent*>(corpse_slot.getData());
			if(corpse.faction_id >= world.data.factions.size()
				|| system.faction_id >= world.data.factions.size()) continue;
			const bool is_ally = corpse.faction_id == system.faction_id;
			const bool is_hostile = world.data.factions.getRelation(
				system.faction_id, corpse.faction_id).type == FactionRelationType::hostile;
			if(!is_ally && !is_hostile) continue;
			const DamageableComponent* health = findDamageable(world, corpse_slot.chunk_id, corpse_slot.point_id);
			if(health == nullptr || health->getHealth() > 0.0f) continue;
			const glm::dvec3 corpse_position = world.getPointWorldPosition(corpse_slot.chunk_id, corpse_slot.point_id);
			const glm::dvec3 search_offset = corpse_position - search_position;
			if(glm::dot(search_offset, search_offset) > search_radius_squared) continue;
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

	/** @brief Orders available units to inspect lost-contact locations after ten seconds without sightings. */
	void assignSearchOrders(World& world, UnitSystem& system,
		const DynamicArray<PointComponentSlotId>& slot_ids)
	{
		if(system.time_since_enemy_visible < 10.0f) return;
		for(size_t index = 0; index < system.search_locations.size();)
		{
			UnitSearchLocation& location = system.search_locations[index];
			if(location.assigned_controller_slot_id != static_cast<PointComponentSlotId>(-1))
			{
				PointComponentSlot* controller_slot = nullptr;
				NpcControllerComponent* controller = nullptr;
				HumanComponent* human = nullptr;
				if(!getActiveNpc(world, location.assigned_controller_slot_id, controller_slot, controller, human)
					|| human->faction_id != system.faction_id
					|| world.data.getChunk(controller_slot->chunk_id).world_body_id != location.body_id)
				{
					location.assigned_controller_slot_id = static_cast<PointComponentSlotId>(-1);
				}
				else if(!controller->order.has_attack_target && world.data.isWorldBodyActive(location.body_id))
				{
					const glm::dvec3 target_position = getSearchWorldPosition(world, location);
					const glm::dvec3 own_position = world.getPointWorldPosition(controller_slot->chunk_id, controller_slot->point_id);
					const glm::dvec3 offset = target_position - own_position;
					if(glm::dot(offset, offset) <= 1.5 * 1.5)
					{
						removeCorpsesAtSearchLocation(world, system, *controller_slot, *human, location, target_position);
						for(size_t shift = index + 1; shift < system.search_locations.size(); ++shift)
							system.search_locations[shift - 1] = static_cast<UnitSearchLocation&&>(system.search_locations[shift]);
						system.search_locations.resize(system.search_locations.size() - 1);
						continue;
					}
					controller->order.has_move_target = true;
					controller->order.move_target_world = target_position;
					controller->order.has_look_target = true;
					controller->order.look_target_world = target_position;
					++index;
					continue;
				}
			}

			if(location.assigned_controller_slot_id == static_cast<PointComponentSlotId>(-1))
			{
				const glm::dvec3 target_position = getSearchWorldPosition(world, location);
				PointComponentSlotId nearest_controller_id = static_cast<PointComponentSlotId>(-1);
				PointComponentSlot* nearest_controller_slot = nullptr;
				NpcControllerComponent* nearest_controller = nullptr;
				HumanComponent* nearest_human = nullptr;
				double nearest_distance_squared = std::numeric_limits<double>::max();
				for(PointComponentSlotId id : slot_ids)
				{
					PointComponentSlot* candidate_slot = nullptr;
					NpcControllerComponent* candidate_controller = nullptr;
					HumanComponent* candidate_human = nullptr;
					if(!getActiveNpc(world, id, candidate_slot, candidate_controller, candidate_human)
						|| candidate_human->faction_id != system.faction_id
						|| world.data.getChunk(candidate_slot->chunk_id).world_body_id != location.body_id
						|| candidate_controller->order.has_attack_target) continue;
					bool already_assigned = false;
					for(const UnitSearchLocation& other : system.search_locations)
						if(other.assigned_controller_slot_id == id) { already_assigned = true; break; }
					if(already_assigned) continue;
					const glm::dvec3 offset = world.getPointWorldPosition(candidate_slot->chunk_id, candidate_slot->point_id)
						- target_position;
					const double distance_squared = glm::dot(offset, offset);
					if(distance_squared < nearest_distance_squared)
					{
						nearest_distance_squared = distance_squared;
						nearest_controller_id = id;
						nearest_controller_slot = candidate_slot;
						nearest_controller = candidate_controller;
						nearest_human = candidate_human;
					}
				}
				if(nearest_controller_id == static_cast<PointComponentSlotId>(-1)) { ++index; continue; }
				location.assigned_controller_slot_id = nearest_controller_id;
				const glm::dvec3 offset = target_position
					- world.getPointWorldPosition(nearest_controller_slot->chunk_id, nearest_controller_slot->point_id);
				if(glm::dot(offset, offset) <= 1.5 * 1.5)
				{
					removeCorpsesAtSearchLocation(world, system, *nearest_controller_slot,
						*nearest_human, location, target_position);
					for(size_t shift = index + 1; shift < system.search_locations.size(); ++shift)
						system.search_locations[shift - 1] = static_cast<UnitSearchLocation&&>(system.search_locations[shift]);
					system.search_locations.resize(system.search_locations.size() - 1);
					continue;
				}
				nearest_controller->order.has_move_target = true;
				nearest_controller->order.move_target_world = target_position;
				nearest_controller->order.has_look_target = true;
				nearest_controller->order.look_target_world = target_position;
			}
			++index;
		}
	}
}
