#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents inventory slot data and behavior.
	 */
	struct InventorySlot
	{
		static constexpr ItemDefinitionId empty_item_definition_id = 0;

		ItemDefinitionId item_definition_id = empty_item_definition_id;
		uint32 count = 0;
		uint64 instance_state = 0;
		bool is_equipped = false;

		bool isEmpty() const { return item_definition_id == empty_item_definition_id; }
		/**
		 * @brief Resets the inventory slot to its empty state.
		 */
		void clear() { *this = InventorySlot(); }
	};

	/**
	 * @brief Stores an owner’s inventory slots and item instance state.
	 */
	struct InventoryComponent
	{
		DynamicArray<InventorySlot> slots;

		size_t slotCount() const { return slots.size(); }
		/**
		 * @brief Changes the number of inventory slots.
		 */
		void resizeSlots(size_t count) { slots.resize(count); }
	};

	/**
	 * @brief Constructs inventory component.
	 */
	void constructInventoryComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves inventory component.
	 */
	void moveInventoryComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases inventory component.
	 */
	void destroyInventoryComponent(void* data);
}
