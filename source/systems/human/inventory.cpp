#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/item/item.h>

namespace danikk_space_engine
{
	void tickInventory(InventoryComponent& inventory, float dt)
	{
		DSE_WATCH();
		dt = glm::max(dt, 0.0f);
		for(InventorySlot& item : inventory.slots)
		{
			if(item.isEmpty() || item.count == 0
				|| getItemDefinition(item.item_definition_id).category_id != item_category_ids::weapon) continue;
			item.instance_state.weapon.cooldown = glm::max(0.0f,
				item.instance_state.weapon.cooldown - dt);
		}
	}
}
