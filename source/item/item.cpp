#include <danikk_space_engine/item/item.h>

#include <cmath>

namespace danikk_space_engine
{
	/** @brief Adds a uniformly sampled angular offset around a normalized direction. */
	vec3 applyAngularSpread(const vec3& direction, float spread_degrees)
	{
		const float direction_length = glm::length(direction);
		if(direction_length <= 1e-6f || spread_degrees <= 0.0f) return direction;

		const vec3 forward = direction / direction_length;
		const vec3 reference = std::abs(forward.z) < 0.9f ? vec3(0.0f, 0.0f, 1.0f) : vec3(0.0f, 1.0f, 0.0f);
		const vec3 tangent = glm::normalize(glm::cross(forward, reference));
		const vec3 bitangent = glm::cross(forward, tangent);
		const float angle = glm::radians(spread_degrees * (default_random.nextFloat() * 0.5f + 0.5f));
		const float azimuth = default_random.nextFloat() * pi;
		const vec3 offset = tangent * std::cos(azimuth) + bitangent * std::sin(azimuth);
		return glm::normalize(forward * std::cos(angle) + offset * std::sin(angle));
	}

	/** @brief Dispatches stack use to its category callback when available. */
	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot)
	{
		if(slot.isEmpty() || slot.count == 0) return false;
		const ItemDefinition& definition = getItemDefinition(slot.item_definition_id);
		const ItemCategoryDefinition& category = getItemCategoryDefinition(definition.category_id);
		return category.use != nullptr && category.use(world, owner_point_id, slot, definition);
	}

	/** @brief Dispatches item equip to the category callback. */
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemCategoryDefinition& category = getItemCategoryDefinition(definition.category_id);
		return category.equip != nullptr && category.equip(world, owner_point_id, definition);
	}

	/** @brief Dispatches item unequip to the category callback. */
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemCategoryDefinition& category = getItemCategoryDefinition(definition.category_id);
		return category.unequip != nullptr && category.unequip(world, owner_point_id, definition);
	}

	/** @brief Dispatches rendering to the category's draw callback. */
	void drawItem(ItemDefinitionId id, const mat4& world_matrix)
	{
		const ItemDefinition& const_definition = getItemDefinition(id);
		const ItemCategoryDefinition& category = getItemCategoryDefinition(const_definition.category_id);
		if(category.draw == nullptr) formatFatalError("Item category % has no draw function", category.name);
		category.draw(const_cast<ItemDefinition&>(const_definition), world_matrix);
	}
}
