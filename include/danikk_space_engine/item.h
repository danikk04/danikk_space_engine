#pragma once

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	namespace item_type_ids
	{
		inline constexpr ItemTypeId none = 0;
		inline constexpr ItemTypeId weapon = 1;
		inline constexpr ItemTypeId count = 2;
	}

	namespace item_definition_ids
	{
		inline constexpr ItemDefinitionId none = 0;
		inline constexpr ItemDefinitionId mg42 = 1;
		inline constexpr ItemDefinitionId count = 2;
	}

	/**
	 * @brief Stores the damage, armor penetration, and firing values used by a weapon item.
	 */
	struct WeaponItemParameters
	{
		float damage;
		float armor_penetration;
		float caliber_mm;
		float rounds_per_minute;
	};

	/**
	 * @brief Stores the health and nourishment values restored by a food item.
	 */
	struct FoodItemParameters
	{
		float health_restored;
		float hunger_restored;
	};

	/**
	 * @brief Stores damage resistance and movement penalty values for armor.
	 */
	struct ArmorItemParameters
	{
		float physical_resistance;
		float thermal_resistance;
		float speed_debuff;
	};

	union ItemDefinitionParameters
	{
		WeaponItemParameters weapon;
		FoodItemParameters food;
		ArmorItemParameters armor;
		ItemDefinitionParameters() : weapon{} {}
	};

	/**
	 * @brief Describes a concrete item, its visual assets, and its parameters.
	 */
	struct ItemDefinition;

	using ItemUseFunction = bool (*)(World& world, PointId owner_point_id,
		InventorySlot& slot, const ItemDefinition& definition);
	using ItemEquipFunction = bool (*)(World& world, PointId owner_point_id,
		const ItemDefinition& definition);
	using ItemDrawFunction = void (*)(ItemDefinition& definition, const mat4& world_matrix);

	/**
	 * @brief Defines operations supported by a category of items.
	 */
	struct ItemType
	{
		const char* name = "";
		ItemUseFunction use = nullptr;
		ItemEquipFunction equip = nullptr;
		ItemEquipFunction unequip = nullptr;
		ItemDrawFunction draw = nullptr;
	};

	/**
	 * @brief Describes a concrete item, its visual assets, and its parameters.
	 */
	struct ItemDefinition
	{
		ItemTypeId type_id = item_type_ids::none;
		const char* name = "";
		uint32 max_stack = 1;
		Mesh mesh;
		Texture texture;
		vec4 render_color = vec4(1.0f);
		ItemDefinitionParameters parameters;
	};

	/**
	 * @brief Loads item catalog.
	 */
	void loadItemCatalog();

	/**
	 * @brief Returns the registered item behavior table for the type ID.
	 */
	const ItemType& getItemType(ItemTypeId id);

	/**
	 * @brief Returns the item definition stored at the requested item definition ID.
	 */
	const ItemDefinition& getItemDefinition(ItemDefinitionId id);

	/**
	 * @brief Returns the weapon parameter record after confirming that the definition is a weapon.
	 */
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id);

	/**
	 * @brief Invokes the item type’s use callback for the selected inventory stack.
	 */
	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot);

	/**
	 * @brief Equips the selected stack and invokes the item type’s equip callback.
	 */
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id);

	/**
	 * @brief Unequips the item and invokes the item type’s unequip callback.
	 */
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id);

	/**
	 * @brief Draws item.
	 */
	void drawItem(ItemDefinitionId id, const mat4& world_matrix);
}
