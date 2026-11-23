#include <danikk_space_engine/item/category.h>
#include <danikk_space_engine/item/item.h>

#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/item/definition.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/human/system.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Resolves a human by its independent HumanSystem identity. */
		HumanData* getHuman(World& world, HumanId human_id)
		{ return world.getHumanSystem().findHuman(human_id); }

		/** @brief Creates a projectile using weapon and owner parameters. */
		bool useWeapon(World& world, HumanId owner_human_id, InventorySlot& slot, const ItemDefinition& definition)
		{
			if(!slot.is_equipped || slot.instance_state.weapon.cooldown > 0.0f
				|| definition.parameters.weapon.projectile_id == projectile_definition_ids::none)
				return false;
			HumanData* human_data = getHuman(world, owner_human_id);
			if(human_data == nullptr || !world.getWorldBodyData().isWorldBodyActive(human_data->world_body_id)) return false;
			HumanState* human = human_data;
			const WorldBodyId body_id = human_data->world_body_id;

			const ProjectileDefinition& projectile = getProjectileDefinition(definition.parameters.weapon.projectile_id);
			const vec3 up = glm::normalize(human->up);
			const vec3 direction = human->getWeaponDirection();
			const glm::dvec3 muzzle = human_data->body_local_position
				+ glm::dvec3(up * (human->getHeight() * 0.2f)
					+ direction * (human->getHeight() * 0.6f));
			BulletComponent bullet;
			const vec3 weapon_direction = applyAngularSpread(direction, definition.parameters.weapon.spread_degrees);
			bullet.velocity = applyAngularSpread(weapon_direction, human->getSpreadDegrees()) * projectile.speed;
			bullet.lifetime = projectile.lifetime;
			bullet.owner_human_id = owner_human_id;
			bullet.damage = definition.parameters.weapon.damage;
			const float rate = definition.parameters.weapon.rounds_per_minute;
			slot.instance_state.weapon.cooldown = rate > 0.0f ? 60.0f / rate : 0.25f;
			spawnBullet(world, body_id, muzzle, bullet);
			return true;
		}

		/** @brief Poses a human's arms to hold a firearm. */
		bool equipWeapon(World& world, HumanId owner_human_id, const ItemDefinition&)
		{
			HumanState* human = getHuman(world, owner_human_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::weapon_ready;
			return true;
		}

		/** @brief Restores a human's relaxed arm pose after unequipping a firearm. */
		bool unequipWeapon(World& world, HumanId owner_human_id, const ItemDefinition&)
		{
			HumanState* human = getHuman(world, owner_human_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::relaxed;
			return true;
		}

		/** @brief Draws the item's mesh using its texture or the default white texture. */
		void drawWeapon(ItemDefinition& definition, const mat4& world_matrix)
		{
			if(definition.mesh.isNull()) formatFatalError("Weapon item has no loaded mesh: %", definition.name);
			if(definition.texture.isNull()) white_texture.bind();
			else definition.texture.bind();
			setDrawColor(definition.render_color);
			setWorldMatrix(world_matrix);
			definition.mesh.draw();
		}

		const ItemCategoryDefinition item_categories[item_category_ids::count] =
		{
			{},
			{"Weapon", useWeapon, equipWeapon, unequipWeapon, drawWeapon}
		};
	}

	/** @brief Returns a category table entry or stops on an invalid category ID. */
	const ItemCategoryDefinition& getItemCategoryDefinition(ItemCategoryId id)
	{
		if(id >= item_category_ids::count) formatFatalError("Invalid item category ID %", id);
		return item_categories[id];
	}
}
