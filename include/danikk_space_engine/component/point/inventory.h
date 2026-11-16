#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents one inventory stack and the state shared by its item instances.
	 *
	 * A slot is empty when `item_definition_id` equals `empty_item_definition_id`; callers
	 * should clear the whole slot when removing its final item so count and equipment state reset.
	 * Since `instance_state` and `is_equipped` apply to the whole stack, items with different
	 * state or equipment status must occupy separate slots. The item definition's max-stack limit
	 * is metadata; this structure does not enforce it automatically.
	 */
	struct InventorySlot
	{
		/** @brief Reserved item-definition ID used to mark an unused slot. */
		static constexpr ItemDefinitionId empty_item_definition_id = 0;

		/** @brief Concrete item definition stored in this stack, or the empty sentinel. */
		ItemDefinitionId item_definition_id = empty_item_definition_id;
		/** @brief Number of items represented by this stack. */
		uint32 count = 0;
		/** @brief Item-type-specific state shared by every item represented by this stack. */
		uint64 instance_state = 0;
		/** @brief Whether this stack is currently worn or held as equipment. */
		bool is_equipped = false;

		/**
		 * @brief Tests whether the slot contains no item definition.
		 * @return True when `item_definition_id` is the reserved empty ID.
		 * @note A non-empty definition with count zero still returns false; callers that consume items
		 *       must also check the count and clear the slot when its last item is removed.
		 */
		bool isEmpty() const { return item_definition_id == empty_item_definition_id; }
		/**
		 * @brief Resets the definition, count, instance state, and equipment flag to defaults.
		 */
		void clear() { *this = InventorySlot(); }
	};

	/**
	 * @brief Owns a dynamic sequence of item stacks for the point carrying this component.
	 *
	 * The inventory is generic and can be attached to people, machines, or storage points;
	 * item behavior and render data are resolved through the item-definition catalog.
	 */
	struct InventoryComponent
	{
		/** @brief Ordered item slots owned by this inventory component. */
		DynamicArray<InventorySlot> slots;

		/**
		 * @brief Returns the number of allocated inventory slots.
		 * @return Current `slots` length, including empty slots.
		 */
		size_t slotCount() const { return slots.size(); }
		/**
		 * @brief Resizes the inventory, preserving existing slots up to the new length.
		 * @param count Desired number of slots; newly added entries are default-empty.
		 * @note Does not merge stacks, enforce item max-stack limits, or compact empty slots.
		 */
		void resizeSlots(size_t count) { slots.resize(count); }
	};

	/**
	 * @brief Placement-constructs an inventory, copying optional initial slot contents.
	 * @param destination Address of storage supplied by the point-component slot.
	 * @param init_data Optional pointer to an `InventoryComponent`; null creates an empty inventory.
	 */
	void constructInventoryComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs an inventory payload during slot relocation.
	 * @param destination Address of replacement payload storage.
	 * @param source Existing payload, whose destructor is run after the move.
	 */
	void moveInventoryComponent(void* destination, void* source);

	/**
	 * @brief Runs the inventory destructor, releasing its dynamic slot array.
	 * @param data Address of the live inventory payload.
	 */
	void destroyInventoryComponent(void* data);
}
