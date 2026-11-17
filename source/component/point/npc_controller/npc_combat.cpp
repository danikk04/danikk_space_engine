#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine::npc_internal
{
	/** @brief Executes only the equipment and firing actions present in the assigned UnitOrder. */
	void executeNpcCombatOrder(World& world, PointComponentSlot& slot, NpcControllerComponent& controller)
	{
		DSE_WATCH();
		InventoryComponent* inventory = nullptr;
		for(PointComponentSlotId id : world.data.getChunk(slot.chunk_id).point_component_slot_ids)
		{
			if(!world.data.isPointComponentSlotActive(id)) continue;
			PointComponentSlot& component = world.data.getPointComponentSlot(id);
			if(component.point_id != slot.point_id || component.type_id != PointComponentTypeId::inventory) continue;
			inventory = static_cast<InventoryComponent*>(component.getData());
			break;
		}
		if(inventory == nullptr) return;

		if(controller.order.has_equip_order)
		{
			for(InventorySlot& item : inventory->slots)
			{
				if(item.isEmpty()) continue;
				if(item.is_equipped && item.item_definition_id != controller.order.equip_item_definition_id)
				{
					if(unequipItem(world, slot.point_id, item.item_definition_id)) item.is_equipped = false;
				}
				if(item.item_definition_id == controller.order.equip_item_definition_id && !item.is_equipped)
				{
					if(equipItem(world, slot.point_id, item.item_definition_id)) item.is_equipped = true;
				}
			}
		}

		if(!controller.order.should_fire) return;
		for(InventorySlot& item : inventory->slots)
		{
			if(item.isEmpty() || !item.is_equipped
				|| item.item_definition_id != controller.order.fire_item_definition_id) continue;
			useItem(world, slot.point_id, item);
			return;
		}
	}
}
