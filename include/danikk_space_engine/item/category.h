#pragma once

#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Category IDs index the registered category definitions. */
	namespace item_category_ids
	{
		inline constexpr ItemCategoryId none = 0;
		inline constexpr ItemCategoryId weapon = 1;
		inline constexpr ItemCategoryId count = 2;
	}

	using ItemUseFunction = bool (*)(World&, HumanId, InventorySlot&, const ItemDefinition&);
	using ItemEquipFunction = bool (*)(World&, HumanId, const ItemDefinition&);
	using ItemDrawFunction = void (*)(ItemDefinition&, const mat4&);

	/** @brief Shared behavior callbacks for one category of concrete items. */
	struct ItemCategoryDefinition
	{
		const char* name = ""; ///< Human-readable category name.
		ItemUseFunction use = nullptr; ///< Optional callback for using an inventory stack.
		ItemEquipFunction equip = nullptr; ///< Optional callback when equipping an item.
		ItemEquipFunction unequip = nullptr; ///< Optional callback when unequipping an item.
		ItemDrawFunction draw = nullptr; ///< Callback used to render items in this category.
	};

	/** @brief Returns a registered category definition by its catalog index. */
	const ItemCategoryDefinition& getItemCategoryDefinition(ItemCategoryId id);
}
