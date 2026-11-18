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
	void assignOrder(World& world, PointComponentSlot& controller_slot,
		NpcControllerComponent& controller, HumanComponent& human,
		DynamicArray<UnitSighting>& known_sightings)
	{
		controller.order.clear();
		const glm::dvec3 own_position = world.getPointWorldPosition(controller_slot.chunk_id, controller_slot.point_id);
		const WorldBodyId own_body_id = world.data.getChunk(controller_slot.chunk_id).world_body_id;
		const UnitSighting* closest = nullptr;
		double closest_distance_squared = std::numeric_limits<double>::max();
		for(const UnitSighting& sighting : known_sightings)
		{
			if(sighting.faction_id != human.faction_id || !isValidSighting(world, sighting)) continue;
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
			rememberSighting(known_sightings, human.faction_id,
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
			assignOrder(world, *controller_slot, *controller, *human, system.known_sightings);
		}
	}
}
