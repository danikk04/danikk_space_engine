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

		bool equipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::weapon_ready;
			return true;
		}

		bool unequipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::relaxed;
			return true;
		}

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

		void validateItemCatalog()
		{
			if(!item_catalog_loaded) formatFatalError("Item catalog has not been loaded");
		}
	}

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

	const ItemType& getItemType(ItemTypeId id)
	{
		if(id >= item_type_ids::count) formatFatalError("Invalid item type ID %", id);
		return item_types[id];
	}

	const ItemDefinition& getItemDefinition(ItemDefinitionId id)
	{
		validateItemCatalog();
		if(id == item_definition_ids::none || id >= item_definition_ids::count)
			formatFatalError("Invalid item definition ID %", id);
		return item_definitions[id];
	}

	const WeaponItemParameters& getWeaponItemParameters(ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		if(definition.type_id != item_type_ids::weapon)
			formatFatalError("Item definition % is not a weapon", id);
		return definition.parameters.weapon;
	}

	bool useItem(World& world, PointId owner_point_id, InventorySlot& slot)
	{
		if(slot.isEmpty() || slot.count == 0) return false;
		const ItemDefinition& definition = getItemDefinition(slot.item_definition_id);
		const ItemType& type = getItemType(definition.type_id);
		return type.use != nullptr && type.use(world, owner_point_id, slot, definition);
	}

	bool equipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemType& type = getItemType(definition.type_id);
		return type.equip != nullptr && type.equip(world, owner_point_id, definition);
	}

	bool unequipItem(World& world, PointId owner_point_id, ItemDefinitionId id)
	{
		const ItemDefinition& definition = getItemDefinition(id);
		const ItemType& type = getItemType(definition.type_id);
		return type.unequip != nullptr && type.unequip(world, owner_point_id, definition);
	}

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
