#include <cmath>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item/item.h>

#include "internal.h"

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		/** @brief Finds an inventory attached to a unit's point. */
		InventoryComponent* findUnitInventory(World& world, ChunkId chunk_id, PointId point_id)
		{
			DSE_WATCH();
			for(PointComponentSlotId id : world.chunks.getChunk(chunk_id).point_component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(id)) continue;
				PointComponentSlot& slot = world.point_data.getPointComponentSlot(id);
				if(slot.point_id == point_id && slot.type_id == PointComponentTypeId::inventory)
					return static_cast<InventoryComponent*>(slot.getData());
			}
			return nullptr;
		}

		/** @brief Applies movement, look, equipment, and firing actions stored in one unit order. */
		void executeOrder(World& world, PointComponentSlot& slot, UnitAgentState& agent, HumanComponent& human)
		{
			DSE_WATCH();
			const float dt = glm::max(getTargetFrameDelay(), 0.0f);
			const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
			const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 own_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			human.desired_velocity = vec3(0.0f);
			if(agent.order.has_move_target)
			{
				const glm::dvec3 body_local_target = glm::conjugate(body.rotation)
					* (agent.order.move_target_world - body.position);
				const vec3 target_in_chunk = vec3(body_local_target - chunk.position);
				vec3 offset = target_in_chunk - world.point_data.getPoint(slot.point_id).position;
				offset -= human.up * glm::dot(offset, human.up);
				const float distance = glm::length(offset);
				if(distance > 0.02f && dt > 0.0f)
					human.desired_velocity = offset / distance
						* glm::min(glm::max(human.getMovementSpeed(), 0.0f), distance / dt);
			}
			if(agent.order.has_look_target)
			{
				const glm::dvec3 head_position = own_position
					+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.375f));
				const glm::dvec3 target_offset = agent.order.look_target_world - head_position;
				const vec3 direction = vec3(glm::conjugate(body.rotation) * target_offset);
				human.lookAtDirection(direction, static_cast<float>(glm::length(target_offset)));
			}

			InventoryComponent* inventory = findUnitInventory(world, slot.chunk_id, slot.point_id);
			if(inventory == nullptr) return;
			if(agent.order.has_equip_order)
				for(InventorySlot& item : inventory->slots)
				{
					if(item.isEmpty()) continue;
					if(item.is_equipped && item.item_definition_id != agent.order.equip_item_definition_id)
					{
						if(unequipItem(world, slot.point_id, item.item_definition_id)) item.is_equipped = false;
					}
					if(item.item_definition_id == agent.order.equip_item_definition_id && !item.is_equipped)
						if(equipItem(world, slot.point_id, item.item_definition_id)) item.is_equipped = true;
				}
			if(!agent.order.should_fire) return;
			for(InventorySlot& item : inventory->slots)
			{
				if(item.isEmpty() || !item.is_equipped
					|| item.item_definition_id != agent.order.fire_item_definition_id) continue;
				useItem(world, slot.point_id, item);
				return;
			}
		}
	}

	/** @brief Moves a unit and all its component slots into the loaded chunk containing its point. */
	void updateUnitChunk(World& world, PointComponentSlot& human_slot)
	{
		DSE_WATCH();
		if(!world.point_data.isPointActive(human_slot.point_id)) return;
		Chunk& old_chunk = world.chunks.getChunk(human_slot.chunk_id);
		WorldBody& body = world.world_body_data.getWorldBody(old_chunk.world_body_id);
		const PointId point_id = human_slot.point_id;
		const glm::dvec3 position = old_chunk.position + glm::dvec3(world.point_data.getPoint(point_id).position);
		const glm::ivec3 coordinate = glm::ivec3(glm::floor(position / planet_chunk_width));
		ChunkId new_chunk_id = static_cast<ChunkId>(-1);
		PlanetComponent* planet = nullptr;
		for(WorldBodyComponentSlotId id : body.component_slot_ids)
		{
			if(!world.world_body_data.isWorldBodyComponentSlotActive(id)) continue;
			WorldBodyComponentSlot& component = world.world_body_data.getWorldBodyComponentSlot(id);
			if(component.type_id != WorldBodyComponentTypeId::planet) continue;
			planet = static_cast<PlanetComponent*>(component.getData());
			planet->chunk_lookup.getChunk(coordinate, new_chunk_id);
			break;
		}
		if(planet == nullptr || new_chunk_id == human_slot.chunk_id) return;
		if(!world.chunks.isChunkActive(new_chunk_id))
		{
			const glm::dvec3 minimum = glm::dvec3(old_chunk.grid_coordinate) * planet_chunk_width;
			const glm::dvec3 maximum = minimum + glm::dvec3(planet_chunk_width - 0.001);
			world.point_data.getPoint(point_id).position = vec3(glm::clamp(position, minimum, maximum) - old_chunk.position);
			return;
		}

		DynamicArray<PointComponentSlotId> moved_slots;
		for(size_t i = 0; i < old_chunk.point_component_slot_ids.size();)
		{
			const PointComponentSlotId id = old_chunk.point_component_slot_ids[i];
			if(world.point_data.getPointComponentSlot(id).point_id != point_id) { ++i; continue; }
			moved_slots.push(id);
			world.chunks.removePointComponentSlotFromChunk(id);
		}
		for(size_t i = 0; i < old_chunk.point_ids.size(); ++i)
		{
			if(old_chunk.point_ids[i] != point_id) continue;
			for(size_t j = i; j + 1 < old_chunk.point_ids.size(); ++j)
				old_chunk.point_ids[j] = old_chunk.point_ids[j + 1];
			old_chunk.point_ids.resize(old_chunk.point_ids.size() - 1);
			world.point_data.getPoint(point_id).referencing_chunk_count--;
			break;
		}
		world.point_data.getPoint(point_id).position = vec3(position - world.chunks.getChunk(new_chunk_id).position);
		for(PointComponentSlotId id : moved_slots) world.chunks.addPointComponentSlotToChunk(new_chunk_id, id);
	}

	/** @brief Executes current unit orders and then updates each unit's chunk ownership. */
	void executeUnitOrders(World& world, UnitSystem& system)
	{
		DSE_WATCH();
		for(UnitAgentState& agent : system.agents)
		{
			PointComponentSlot* slot = nullptr;
			HumanComponent* human = nullptr;
			if(!getActiveUnit(world, agent, slot, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			executeOrder(world, *slot, agent, *human);
			updateUnitChunk(world, *slot);
		}
	}
}
