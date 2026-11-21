#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Mutable state belonging to one weapon instance. */
	struct WeaponItemState
	{
		float cooldown = 0.0f; ///< Seconds remaining until the weapon may fire.
		uint32 ammunition = 0; ///< Reserved for ammunition; weapons currently use an unlimited supply.
	};

	/** @brief Category-specific item state; select the member matching the item type. */
	union ItemInstanceState
	{
		WeaponItemState weapon; ///< State used by weapon items.
		/** @brief Initializes weapon state to an empty magazine and no cooldown. */
		ItemInstanceState() : weapon{} {}
	};

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
		ItemInstanceState instance_state;
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
	 * @brief Owns a dynamic sequence of item stacks for one entity.
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

	/** @brief Advances weapon cooldowns for an inventory owned outside the point-component system. */
	void tickInventory(InventoryComponent& inventory, float dt);

}
