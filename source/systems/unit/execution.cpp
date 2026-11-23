#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/systems/unit/internal/orders.h>
#include <danikk_space_engine/systems/unit/internal/state.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/item/item.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine::unit_system_internal
{
	namespace
	{
		/** @brief Applies movement, look, equipment, and firing actions stored in one unit order. */
		void executeOrder(World& world, HumanData& data, UnitAgentState& agent, HumanState& human)
		{
			DSE_WATCH();
			const float dt = glm::max(getTargetFrameDelay(), 0.0f);
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(data.world_body_id);
			const glm::dvec3 own_position = world.getHumanSystem().getWorldPosition(data);
			human.desired_velocity = vec3(0.0f);
			if(agent.order.has_move_target)
			{
				const glm::dvec3 body_local_target = glm::conjugate(body.rotation)
					* (agent.order.move_target_world - body.position);
				vec3 offset = vec3(body_local_target - data.body_local_position);
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

			InventoryComponent* inventory = &data.inventory;
			if(agent.order.has_equip_order)
				for(InventorySlot& item : inventory->slots)
				{
					if(item.isEmpty()) continue;
					if(item.is_equipped && item.item_definition_id != agent.order.equip_item_definition_id)
					{
						if(unequipItem(world, data.id, item.item_definition_id)) item.is_equipped = false;
					}
					if(item.item_definition_id == agent.order.equip_item_definition_id && !item.is_equipped)
						if(equipItem(world, data.id, item.item_definition_id)) item.is_equipped = true;
				}
			if(!agent.order.should_fire) return;
			for(InventorySlot& item : inventory->slots)
			{
				if(item.isEmpty() || !item.is_equipped
					|| item.item_definition_id != agent.order.fire_item_definition_id) continue;
				useItem(world, data.id, item);
				return;
			}
		}
	}

	/** @brief Executes current unit orders against body-local positions. */
	void executeUnitOrders(World& world, UnitGroup& system)
	{
		DSE_WATCH();
		for(UnitAgentState& agent : system.agents)
		{
			HumanData* human = nullptr;
			if(!getActiveUnit(world, agent, human)) continue;
			if(human->faction_id != system.faction_id) continue;
			executeOrder(world, *human, agent, *human);
		}
	}
}
