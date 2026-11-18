#include <danikk_space_engine/item/definition.h>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	namespace
	{
		const ProjectileDefinition projectile_definitions[] = {{0.0f, 0.0f}, {120.0f, 5.0f}};
		ItemDefinition item_definitions[item_definition_ids::count];
		bool item_catalog_loaded = false;
	}

	/** @brief Loads the MG-42 mesh and initializes the concrete item catalog. */
	void loadItemCatalog()
	{
		ItemDefinition& mg42 = item_definitions[item_definition_ids::mg42];
		mg42.category_id = item_category_ids::weapon;
		mg42.name = "MG-42";
		mg42.max_stack = 1;
		mg42.mesh.loadObj("MG-42", "MG-42");
		mg42.render_color = vec4(0.4f, 0.4f, 0.4f, 1.0f);
		mg42.parameters.weapon = WeaponItemParameters{projectile_definition_ids::mg42_bullet, 25.0f, 0.0f, 0.0f, 240.0f, 1.0f};
		item_catalog_loaded = true;
	}

	/** @brief Returns a projectile definition after validating its ID. */
	const ProjectileDefinition& getProjectileDefinition(ProjectileDefinitionId id)
	{
		if(id == projectile_definition_ids::none || id >= sizeof(projectile_definitions) / sizeof(projectile_definitions[0]))
			formatFatalError("Invalid projectile definition ID %", id);
		return projectile_definitions[id];
	}

	/** @brief Returns a loaded concrete item definition after validating catalog state and ID. */
	const ItemDefinition& getItemDefinition(ItemDefinitionId id)
	{
		if(!item_catalog_loaded) formatFatalError("Item catalog has not been loaded");
		if(id == item_definition_ids::none || id >= item_definition_ids::count)
			formatFatalError("Invalid item definition ID %", id);
		return item_definitions[id];
	}

	/** @brief Returns weapon parameters after checking that the definition is a weapon. */
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		if(definition.category_id != item_category_ids::weapon)
			formatFatalError("Item definition % is not a weapon", id);
		return definition.parameters.weapon;
	}
}
