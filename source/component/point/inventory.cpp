#include <danikk_space_engine/component/point/inventory.h>

namespace danikk_space_engine
{

	/**
	 * @brief Constructs inventory component.
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
	 * @brief Moves inventory component.
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
	 * @brief Removes and releases inventory component.
	 */
	void destroyInventoryComponent(void* data)
	{
		if(data == nullptr)
			formatFatalError("Inventory component destruction received null memory");
		static_cast<InventoryComponent*>(data)->~InventoryComponent();
	}
}
