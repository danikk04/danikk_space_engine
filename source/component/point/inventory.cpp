#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{

	/**
	 * @brief Constructs an empty inventory or copies the supplied slot array into slot storage.
	 * @param destination Destination storage owned by the component slot.
	 * @param init_data Optional source `InventoryComponent`; null creates an empty slot array.
	 */
	void constructInventoryComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
			formatFatalError("Inventory component construction received null destination");
		if(init_data == nullptr)
			new (destination) InventoryComponent();
		else
			new (destination) InventoryComponent(*static_cast<const InventoryComponent*>(init_data));
	}

	/**
	 * @brief Move-constructs the inventory, transferring ownership of its DynamicArray allocation.
	 * @param destination Replacement component payload storage.
	 * @param source Existing inventory payload, destroyed after its array has been moved.
	 */
	void moveInventoryComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
			formatFatalError("Inventory component move received null memory");
		InventoryComponent* source_inventory = static_cast<InventoryComponent*>(source);
		new (destination) InventoryComponent(static_cast<InventoryComponent&&>(*source_inventory));
		source_inventory->~InventoryComponent();
	}

	/**
	 * @brief Destroys the inventory and its owned slot array without freeing slot-owned storage.
	 * @param data Existing live inventory payload.
	 */
	void destroyInventoryComponent(void* data)
	{
		if(data == nullptr)
			formatFatalError("Inventory component destruction received null memory");
		static_cast<InventoryComponent*>(data)->~InventoryComponent();
	}

	/** @copydoc tickInventoryComponent */
	void tickInventoryComponent(PointComponentSlot& slot, World&)
	{
		InventoryComponent& inventory = *static_cast<InventoryComponent*>(slot.getData());
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		for(InventorySlot& item : inventory.slots)
		{
			if(item.isEmpty() || item.count == 0 || getItemDefinition(item.item_definition_id).type_id != item_type_ids::weapon) continue;
			item.instance_state.weapon.cooldown = glm::max(0.0f, item.instance_state.weapon.cooldown - dt);
		}
	}
}
