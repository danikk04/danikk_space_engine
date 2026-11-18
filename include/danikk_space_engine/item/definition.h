#pragma once

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture.h>
#include <danikk_space_engine/item/category.h>

namespace danikk_space_engine
{
	/** @brief IDs index the concrete item-definition catalog. */
	namespace item_definition_ids
	{
		inline constexpr ItemDefinitionId none = 0;
		inline constexpr ItemDefinitionId mg42 = 1;
		inline constexpr ItemDefinitionId count = 2;
	}

	/** @brief IDs index projectile parameter definitions. */
	namespace projectile_definition_ids
	{
		inline constexpr ProjectileDefinitionId none = 0;
		inline constexpr ProjectileDefinitionId mg42_bullet = 1;
	}

	/** @brief Speed and lifetime parameters for a projectile. */
	struct ProjectileDefinition
	{
		float speed; ///< Initial speed in metres per second.
		float lifetime; ///< Maximum lifetime in seconds.
	};

	/** @brief Returns projectile parameters for a valid nonzero ID. */
	const ProjectileDefinition& getProjectileDefinition(ProjectileDefinitionId id);

	/** @brief Numeric parameters shared by weapon definitions. */
	struct WeaponItemParameters
	{
		ProjectileDefinitionId projectile_id; ///< Projectile spawned by this weapon.
		float damage; ///< Base damage per hit.
		float armor_penetration; ///< Penetration capability against armor.
		float caliber_mm; ///< Ammunition caliber in millimetres.
		float rounds_per_minute; ///< Nominal firing rate.
		float spread_degrees; ///< Maximum weapon firing deviation in degrees.
	};

	/** @brief Numeric parameters reserved for food items. */
	struct FoodItemParameters
	{
		float health_restored; ///< Health restored when consumed.
		float hunger_restored; ///< Hunger reserve restored when consumed.
	};

	/** @brief Numeric parameters reserved for armor items. */
	struct ArmorItemParameters
	{
		float physical_resistance; ///< Resistance to physical damage.
		float thermal_resistance; ///< Resistance to thermal damage.
		float speed_debuff; ///< Movement-speed penalty while worn.
	};

	/** @brief Category-specific parameters; use the member matching the item's category. */
	union ItemDefinitionParameters
	{
		WeaponItemParameters weapon;
		FoodItemParameters food;
		ArmorItemParameters armor;
		ItemDefinitionParameters() : weapon{} {}
	};

	/** @brief A concrete item with category, visual assets, stack limit, and parameters. */
	struct ItemDefinition
	{
		ItemCategoryId category_id = item_category_ids::none; ///< Category selecting shared behavior.
		const char* name = ""; ///< Human-readable item name.
		uint32 max_stack = 1; ///< Maximum identical items in an inventory stack.
		Mesh mesh; ///< Mesh owned by this definition.
		Texture texture; ///< Optional texture; null uses the default white texture.
		vec4 render_color = vec4(1.0f); ///< Tint applied while rendering.
		ItemDefinitionParameters parameters; ///< Numeric data interpreted by category.
	};

	/** @brief Loads visual assets and initializes the concrete item catalog. */
	void loadItemCatalog();
	/** @brief Returns a loaded concrete item definition. */
	const ItemDefinition& getItemDefinition(ItemDefinitionId id);
	/** @brief Returns weapon parameters after validating the definition category. */
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id);
}
