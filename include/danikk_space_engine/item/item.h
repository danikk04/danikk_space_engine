#pragma once

#include <danikk_space_engine/item/category.h>
#include <danikk_space_engine/item/definition.h>

namespace danikk_space_engine
{
	/** @brief Randomly deflects a direction by at most the requested angle. */
	vec3 applyAngularSpread(const vec3& direction, float spread_degrees);
	/** @brief Uses the category callback for a nonempty inventory stack. */
	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot);
	/** @brief Invokes the item's category equip callback. */
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id);
	/** @brief Invokes the item's category unequip callback. */
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id);
	/** @brief Dispatches the category draw callback with the supplied transform. */
	void drawItem(ItemDefinitionId id, const mat4& world_matrix);
}
