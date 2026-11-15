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

	struct WeaponItemParameters
	{
		float damage;
		float armor_penetration;
		float caliber_mm;
		float rounds_per_minute;
	};

	struct FoodItemParameters
	{
		float health_restored;
		float hunger_restored;
	};

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

	struct ItemDefinition;

	using ItemUseFunction = bool (*)(World& world, PointId owner_point_id,
		InventorySlot& slot, const ItemDefinition& definition);
	using ItemEquipFunction = bool (*)(World& world, PointId owner_point_id,
		const ItemDefinition& definition);
	using ItemDrawFunction = void (*)(ItemDefinition& definition, const mat4& world_matrix);

	struct ItemType
	{
		const char* name = "";
		ItemUseFunction use = nullptr;
		ItemEquipFunction equip = nullptr;
		ItemEquipFunction unequip = nullptr;
		ItemDrawFunction draw = nullptr;
	};

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

	void loadItemCatalog();
	const ItemType& getItemType(ItemTypeId id);
	const ItemDefinition& getItemDefinition(ItemDefinitionId id);
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id);

	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot);
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id);
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id);
	void drawItem(ItemDefinitionId id, const mat4& world_matrix);
}
