#include <danikk_space_engine/item.h>

#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		bool equipWeapon(World& world, PointId owner_point_id, const ItemDefinition& definition);
		bool unequipWeapon(World& world, PointId owner_point_id, const ItemDefinition& definition);
		void drawWeapon(ItemDefinition& definition, const mat4& world_matrix);

		ItemType item_types[item_type_ids::count] =
		{
			{},
			{"Weapon", nullptr, equipWeapon, unequipWeapon, drawWeapon}
		};
		ItemDefinition item_definitions[item_definition_ids::count];
		bool item_catalog_loaded = false;

		/**
		 * @brief Scans active point-component slots and returns the human component attached to the requested point, or nullptr if none exists.
		 */
		HumanComponent* getHumanAtPoint(World& world, PointId point_id)
		{
			for(PointComponentSlotId slot_id : world.data.getUsedPointComponentSlotIds())
			{
				if(!world.data.isPointComponentSlotActive(slot_id)) continue;
				PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
				if(slot.point_id != point_id || slot.type_id != PointComponentTypeId::human) continue;
				return static_cast<HumanComponent*>(slot.getData());
			}
			return nullptr;
		}

		/**
		 * @brief Sets the owning human's arm pose to weapon-ready for a held firearm.
		 * @param world World used to find a HumanComponent attached to the owner point.
		 * @param owner_point_id Point that owns the equipped inventory slot.
		 * @param definition Concrete weapon definition; unused by this shared pose callback.
		 * @return True when a human component was found and its arm pose was changed.
		 */
		bool equipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::weapon_ready;
			return true;
		}

		/**
		 * @brief Restores the owning human's relaxed arm pose after removing a held weapon.
		 * @param world World used to find the human attached to the owner point.
		 * @param owner_point_id Point whose equipped weapon is being removed.
		 * @param definition Concrete weapon definition; unused by this shared pose callback.
		 * @return True when a human component was found and its arm pose was reset.
		 */
		bool unequipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::relaxed;
			return true;
		}

		/**
		 * @brief Binds the definition texture or fallback white texture, applies tint/transform, and draws its mesh.
		 * @param definition Item definition providing the mesh, optional texture, and render tint.
		 * @param world_matrix Transform from item-local model space to render space.
		 */
		void drawWeapon(ItemDefinition& definition, const mat4& world_matrix)
		{
			if(definition.mesh.isNull())
				formatFatalError("Weapon item has no loaded mesh: %", definition.name);
			if(definition.texture.isNull()) white_texture.bind();
			else definition.texture.bind();
			setDrawColor(definition.render_color);
			setWorldMatrix(world_matrix);
			definition.mesh.draw();
		}

		/**
		 * @brief Fails fast when item catalog access occurs before loadItemCatalog() initializes it.
		 */
		void validateItemCatalog()
		{
			if(!item_catalog_loaded) formatFatalError("Item catalog has not been loaded");
		}
	}

	/**
	 * @brief Loads the MG-42 mesh and initializes its weapon category, stack limit, tint, and parameters.
	 *
	 * This catalog currently registers one concrete item; its weapon numeric values are placeholders.
	 * No MG-42 texture is loaded yet, so its draw callback uses the white fallback texture.
	 */
	void loadItemCatalog()
	{
		ItemDefinition& mg42 = item_definitions[item_definition_ids::mg42];
		mg42.type_id = item_type_ids::weapon;
		mg42.name = "MG-42";
		mg42.max_stack = 1;
		mg42.mesh.loadObj("MG-42", "MG-42");
		mg42.render_color = vec4(0.4f, 0.4f, 0.4f, 1.0f);
		mg42.parameters.weapon = WeaponItemParameters{0.0f, 0.0f, 0.0f, 0.0f};
		item_catalog_loaded = true;
	}

	/**
	 * @brief Returns the callback table for the requested item category.
	 * @param id Category index into `item_types`.
	 * @return Static category record; invalid IDs terminate through a fatal error.
	 */
	const ItemType& getItemType(ItemTypeId id)
	{
		if(id >= item_type_ids::count) formatFatalError("Invalid item type ID %", id);
		return item_types[id];
	}

	/**
	 * @brief Returns an initialized concrete item definition after checking catalog state and ID.
	 * @param id Definition index; `none` and out-of-range IDs are invalid.
	 * @return Const definition record with its item assets and category parameters.
	 */
	const ItemDefinition& getItemDefinition(ItemDefinitionId id)
	{
		validateItemCatalog();
		if(id == item_definition_ids::none || id >= item_definition_ids::count)
			formatFatalError("Invalid item definition ID %", id);
		return item_definitions[id];
	}

	/**
	 * @brief Retrieves the weapon parameter union member after checking the item's category.
	 * @param id Concrete item definition expected to be a weapon.
	 * @return Reference to weapon damage, penetration, caliber, and firing-rate values.
	 */
	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		if(definition.type_id != item_type_ids::weapon)
			formatFatalError("Item definition % is not a weapon", id);
		return definition.parameters.weapon;
	}

	/**
	 * @brief Validates a non-empty stack and dispatches its category's use callback if present.
	 * @param world World context for the use operation.
	 * @param owner_point_id Point whose inventory contains the stack.
	 * @param slot Mutable stack passed through to the callback.
	 * @return False for an empty stack or missing callback; otherwise the callback result.
	 */
	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot)
	{
		if(slot.isEmpty() || slot.count == 0) return false;
		const ItemDefinition& definition = getItemDefinition(slot.item_definition_id);
		const ItemType& type = getItemType(definition.type_id);
		return type.use != nullptr && type.use(world, owner_point_id, slot, definition);
	}

	/**
	 * @brief Dispatches the definition's category equip callback for its owner point.
	 * @param world World containing the item owner.
	 * @param owner_point_id Point whose equipped state should be updated.
	 * @param id Concrete item definition to equip.
	 * @return False if the category has no equip callback or the callback rejects the action.
	 */
	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemType& type = getItemType(definition.type_id);
		return type.equip != nullptr && type.equip(world, owner_point_id, definition);
	}

	/**
	 * @brief Dispatches the definition's category unequip callback for its owner point.
	 * @param world World containing the item owner.
	 * @param owner_point_id Point whose equipped state should be cleared.
	 * @param id Concrete item definition to unequip.
	 * @return False if the category has no unequip callback or the callback rejects the action.
	 */
	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemType& type = getItemType(definition.type_id);
		return type.unequip != nullptr && type.unequip(world, owner_point_id, definition);
	}

	/**
	 * @brief Validates the catalog/definition and calls the item's category draw callback.
	 * @param id Concrete item definition to render.
	 * @param world_matrix Transform supplied to the category renderer.
	 */
	void drawItem(ItemDefinitionId id, const mat4& world_matrix)
	{
		validateItemCatalog();
		if(id == item_definition_ids::none || id >= item_definition_ids::count)
			formatFatalError("Invalid item definition ID %", id);
		ItemDefinition& definition = item_definitions[id];
		const ItemType& type = getItemType(definition.type_id);
		if(type.draw == nullptr) formatFatalError("Item type % has no draw function", type.name);
		type.draw(definition, world_matrix);
	}
}
