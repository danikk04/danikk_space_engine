#pragma once

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{
	/** @brief Stable indexes for category-level item behavior callbacks. */
	namespace item_type_ids
	{
		inline constexpr ItemTypeId none = 0; ///< Empty/unregistered category.
		inline constexpr ItemTypeId weapon = 1; ///< Items drawn and equipped through weapon callbacks.
		inline constexpr ItemTypeId count = 2; ///< Number of entries in the item-type table.
	}

	/** @brief Stable indexes for concrete item definitions in the item catalog. */
	namespace item_definition_ids
	{
		inline constexpr ItemDefinitionId none = 0; ///< Reserved empty definition ID.
		inline constexpr ItemDefinitionId mg42 = 1; ///< MG-42 weapon definition.
		inline constexpr ItemDefinitionId count = 2; ///< Number of entries in the item-definition table.
	}

	/**
	 * @brief Stores the damage, armor penetration, and firing values used by a weapon item.
	 */
	struct WeaponItemParameters
	{
		/** @brief Base damage per hit, before target resistance. */
		float damage;
		/** @brief Penetration capability used against armor. */
		float armor_penetration;
		/** @brief Ammunition caliber in millimeters. */
		float caliber_mm;
		/** @brief Nominal firing rate in rounds per minute. */
		float rounds_per_minute;
	};

	/**
	 * @brief Stores the health and nourishment values restored by a food item.
	 */
	struct FoodItemParameters
	{
		/** @brief Health restored when consumed. */
		float health_restored;
		/** @brief Hunger or food reserve restored when consumed. */
		float hunger_restored;
	};

	/**
	 * @brief Stores damage resistance and movement penalty values for armor.
	 */
	struct ArmorItemParameters
	{
		/** @brief Resistance against physical damage. */
		float physical_resistance;
		/** @brief Resistance against thermal damage. */
		float thermal_resistance;
		/** @brief Movement-speed penalty while the armor is worn. */
		float speed_debuff;
	};

	/** @brief Category-specific numeric parameters; interpret only the member matching ItemTypeId. */
	union ItemDefinitionParameters
	{
		WeaponItemParameters weapon; ///< Active when the definition's type is `weapon`.
		FoodItemParameters food; ///< Reserved parameter set for a future food category.
		ArmorItemParameters armor; ///< Reserved parameter set for a future armor category.
		ItemDefinitionParameters() : weapon{} {}
	};

	/**
	 * @brief Describes a concrete item, its visual assets, and its parameters.
	 */
	struct ItemDefinition;

	/**
	 * @brief Applies a category-specific use action to an inventory stack.
	 * @param world World context for the action.
	 * @param owner_point_id Point that owns and uses the stack.
	 * @param slot Mutable stack; the callback may consume items or update shared state.
	 * @param definition Concrete item definition supplying parameters and assets.
	 * @return True when the callback handled the action.
	 */
	using ItemUseFunction = bool (*)(World& world, PointId owner_point_id,
		InventorySlot& slot, const ItemDefinition& definition);
	/**
	 * @brief Applies or removes category-specific equipped state for an item owner.
	 * @param world World containing the owner.
	 * @param owner_point_id Point whose component state may change.
	 * @param definition Concrete definition being equipped or unequipped.
	 * @return True when the state change was handled successfully.
	 */
	using ItemEquipFunction = bool (*)(World& world, PointId owner_point_id,
		const ItemDefinition& definition);
	/**
	 * @brief Draws an item definition at a supplied render transform.
	 * @param definition Concrete definition supplying its mesh and visual settings.
	 * @param world_matrix Transform from item-local space to render space.
	 */
	using ItemDrawFunction = void (*)(ItemDefinition& definition, const mat4& world_matrix);

	/**
	 * @brief Defines shared behavior callbacks for one item category, such as weapons.
	 *
	 * Concrete definitions refer to a category by `type_id`; callbacks receive the concrete
	 * definition so shared behavior can use per-item parameters and render assets.
	 */
	struct ItemType
	{
		/** @brief Display/debug name of this category. */
		const char* name = "";
		/** @brief Optional action applied when an inventory stack is used. */
		ItemUseFunction use = nullptr;
		/** @brief Optional equip behavior for the category. */
		ItemEquipFunction equip = nullptr;
		/** @brief Optional unequip behavior for the category. */
		ItemEquipFunction unequip = nullptr;
		/** @brief Rendering callback shared by all definitions in the category. */
		ItemDrawFunction draw = nullptr;
	};

	/**
	 * @brief Describes a concrete item, its visual assets, and its parameters.
	 */
	struct ItemDefinition
	{
		/** @brief Item category selecting callbacks in the item-type table. */
		ItemTypeId type_id = item_type_ids::none;
		/** @brief Human-readable concrete item name. */
		const char* name = "";
		/** @brief Maximum number of identical instances allowed in one inventory stack. */
		uint32 max_stack = 1;
		/** @brief Mesh owned by this definition and used by its draw callback. */
		Mesh mesh;
		/** @brief Optional texture owned by this definition; null texture uses the default white texture. */
		Texture texture;
		/** @brief Tint multiplied into the item's rendered appearance. */
		vec4 render_color = vec4(1.0f);
		/** @brief Numeric data interpreted according to `type_id`. */
		ItemDefinitionParameters parameters;
	};

	/**
	 * @brief Loads item-owned visual assets and initializes concrete item definitions.
	 *
	 * The current catalog registers only MG-42: it loads the mesh, leaves its
	 * texture null so rendering uses the white fallback, and initializes weapon
	 * numeric parameters to zero placeholders. Call before item lookups or drawing.
	 */
	void loadItemCatalog();

	/**
	 * @brief Returns category callbacks for an item type ID.
	 * @param id Registered item category.
	 * @return Static callback table entry; an invalid ID raises a fatal error.
	 */
	const ItemType& getItemType(ItemTypeId id);

	/**
	 * @brief Returns a loaded concrete item definition by ID.
	 * @param id Concrete definition ID; the reserved `none` ID is not a valid item.
	 * @return Definition containing category, assets, stack limit, and parameters.
	 */
	const ItemDefinition& getItemDefinition(ItemDefinitionId id);

	/**
	 * @brief Retrieves weapon-specific numeric parameters after validating the definition category.
	 * @param id Item definition expected to have weapon category.
	 * @return Reference to the definition's weapon parameter record.
	 */
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id);

	/**
	 * @brief Uses the stack's definition callback when the slot contains at least one item.
	 * @param world World context passed to the item's behavior.
	 * @param owner_point_id Point carrying the inventory and using the item.
	 * @param slot Mutable stack, which the callback may consume or update.
	 * @return True if a use callback exists and reports that it handled the action.
	 */
	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot);

	/**
	 * @brief Invokes the item category's equip behavior for a concrete definition.
	 * @param world World containing the item owner.
	 * @param owner_point_id Point whose state should be changed by equipping.
	 * @param id Definition being equipped.
	 * @return True if the category provides an equip callback and it succeeds.
	 */
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id);

	/**
	 * @brief Invokes the item category's unequip behavior for a concrete definition.
	 * @param world World containing the item owner.
	 * @param owner_point_id Point whose state should be changed by unequipping.
	 * @param id Definition being unequipped.
	 * @return True if the category provides an unequip callback and it succeeds.
	 */
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id);

	/**
	 * @brief Dispatches the definition's category draw callback with its world matrix.
	 * @param id Loaded concrete item definition to draw.
	 * @param world_matrix Transform from item-local coordinates to render coordinates.
	 */
	void drawItem(ItemDefinitionId id, const mat4& world_matrix);
}
