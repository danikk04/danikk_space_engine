#include <danikk_space_engine/item/category.h>
#include <danikk_space_engine/item/item.h>

#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/item/definition.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Finds a human component attached to a point, if present. */
		HumanComponent* getHumanAtPoint(World& world, PointId point_id)
		{
			for(PointComponentSlotId slot_id : world.data.getUsedPointComponentSlotIds())
			{
				if(!world.data.isPointComponentSlotActive(slot_id)) continue;
				PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
				if(slot.point_id == point_id && slot.type_id == PointComponentTypeId::human)
					return static_cast<HumanComponent*>(slot.getData());
			}
			return nullptr;
		}

		/** @brief Creates a projectile using weapon and owner parameters. */
		bool useWeapon(World& world, PointId owner_point_id, InventorySlot& slot, const ItemDefinition& definition)
		{
			if(!slot.is_equipped || slot.instance_state.weapon.cooldown > 0.0f
				|| definition.parameters.weapon.projectile_id == projectile_definition_ids::none)
				return false;
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr || !world.data.isPointActive(owner_point_id)) return false;

			ChunkId chunk_id = static_cast<ChunkId>(-1);
			for(PointComponentSlotId id : world.data.getUsedPointComponentSlotIds())
			{
				const PointComponentSlot& owner = world.data.getPointComponentSlot(id);
				if(owner.point_id == owner_point_id && owner.type_id == PointComponentTypeId::human)
				{
					chunk_id = owner.chunk_id;
					break;
				}
			}
			if(!world.chunk_system.isChunkActive(chunk_id)) return false;

			const ProjectileDefinition& projectile = getProjectileDefinition(definition.parameters.weapon.projectile_id);
			const vec3 up = glm::normalize(human->up);
			const vec3 direction = human->getWeaponDirection();
			const vec3 muzzle = world.data.getPoint(owner_point_id).position
				+ up * (human->getHeight() * 0.2f) + direction * (human->getHeight() * 0.6f);
			BulletComponent bullet;
			const vec3 weapon_direction = applyAngularSpread(direction, definition.parameters.weapon.spread_degrees);
			bullet.velocity = applyAngularSpread(weapon_direction, human->getSpreadDegrees()) * projectile.speed;
			bullet.lifetime = projectile.lifetime;
			bullet.owner_point_id = owner_point_id;
			bullet.damage = definition.parameters.weapon.damage;
			const float rate = definition.parameters.weapon.rounds_per_minute;
			slot.instance_state.weapon.cooldown = rate > 0.0f ? 60.0f / rate : 0.25f;
			spawnBullet(world, chunk_id, muzzle, bullet);
			return true;
		}

		/** @brief Poses a human's arms to hold a firearm. */
		bool equipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
			if(human == nullptr) return false;
			human->arm_pose = HumanArmPose::weapon_ready;
			return true;
		}

		/** @brief Restores a human's relaxed arm pose after unequipping a firearm. */
		bool unequipWeapon(World& world, PointId owner_point_id, const ItemDefinition&)
		{
			HumanComponent* human = getHumanAtPoint(world, owner_point_id);
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
