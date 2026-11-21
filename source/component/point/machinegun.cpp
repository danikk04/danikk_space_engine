#include <utility>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr float bullet_interval = 0.25f;
		constexpr float muzzle_forward_offset = 4.4f;
		constexpr float muzzle_vertical_offset = 0.31f;

		/**
		 * @brief Builds a basis matrix whose columns are the weapon's local forward/up/right axes.
		 * @param machinegun Component carrying the weapon orientation basis.
		 * @return Rotation matrix without translation or scale.
		 */
		mat4 getOrientationMatrix(const MachinegunComponent& machinegun)
		{
			mat4 orientation(1.0f);
			orientation[0] = vec4(machinegun.forward, 0.0f);
			orientation[1] = vec4(machinegun.up, 0.0f);
			orientation[2] = vec4(machinegun.right, 0.0f);
			return orientation;
		}

		/**
		 * @param body_id WorldBody that owns the firing weapon and projectile.
		 * @param position Muzzle position in body-local coordinates.
		 * @param direction Unit travel direction inherited from the weapon barrel.
		 * @param world World used to allocate and register the projectile resources.
		 */
		void spawnMachinegunBullet(WorldBodyId body_id, const glm::dvec3& position,
			const vec3& direction, World& world)
		{
			const WeaponItemParameters& weapon = getWeaponItemParameters(item_definition_ids::mg42);
			const ProjectileDefinition& projectile = getProjectileDefinition(weapon.projectile_id);
			BulletComponent bullet;
			bullet.velocity = applyAngularSpread(direction, weapon.spread_degrees) * projectile.speed;
			bullet.lifetime = projectile.lifetime;
			bullet.damage = weapon.damage;
			spawnBullet(world, body_id, position, bullet);
		}
	}

	/**
	 * @brief Initializes default weapon state or copies caller-provided orientation and timer state.
	 * @param destination Destination storage owned by the component slot.
	 * @param init_data Optional source `MachinegunComponent`; null selects defaults.
	 */
	void constructMachinegunComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component construction received null destination");
		}
		if(init_data == nullptr)
		{
			new (destination) MachinegunComponent();
		}
		else
		{
			new (destination) MachinegunComponent(*static_cast<const MachinegunComponent*>(init_data));
		}
	}

	/**
	 * @brief Transfers the weapon payload to relocated storage and destroys the source object.
	 * @param destination Replacement payload storage.
	 * @param source Existing live machine-gun payload.
	 */
	void moveMachinegunComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component move received null memory");
		}
		MachinegunComponent* source_component = static_cast<MachinegunComponent*>(source);
		new (destination) MachinegunComponent(std::move(*source_component));
		std::destroy_at(source_component);
	}

	/**
	 * @brief Ends the machine-gun payload lifetime; slot storage is reclaimed by PointData.
	 * @param data Existing live machine-gun payload.
	 */
	void destroyMachinegunComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component destruction received null memory");
		}
		std::destroy_at(static_cast<MachinegunComponent*>(data));
	}

	/**
	 * @brief Accumulates elapsed time and spawns a bullet for every elapsed firing interval.
	 * @param slot Machine-gun slot containing payload and attached point ID.
	 * @param world World used to find the owning body and allocate projectile resources.
	 * The interval is 0.25 seconds. Each elapsed interval produces one projectile;
	 * its point is placed at the transformed muzzle and its initial velocity follows
	 * the weapon's forward direction.
	 */
	void tickMachinegunComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();

		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component tick received null data");
		}

		if(!world.point_data.isPointActive(slot.point_id))
		{
			danikk_engine::formatFatalError("Machinegun component references invalid point %", slot.point_id);
		}

		MachinegunComponent& machinegun = *static_cast<MachinegunComponent*>(slot.getData());
		const float delta_time = getTargetFrameDelay();
		if(delta_time <= 0.0f) return;

		float shot_timer = machinegun.shot_timer + delta_time;
		uint32 shot_count = 0;
		while(shot_timer >= bullet_interval)
		{
			shot_timer -= bullet_interval;
			shot_count++;
		}
		machinegun.shot_timer = shot_timer;
		if(shot_count == 0) return;

		const WorldBodyId body_id = slot.world_body_id;
		if(!world.world_body_data.isWorldBodyActive(body_id))
			danikk_engine::formatFatalError("Machinegun component is not attached to a WorldBody");

		const MachinegunComponent machinegun_snapshot = machinegun;
		const mat4 rotation_matrix = getOrientationMatrix(machinegun_snapshot);
		const vec3 direction = machinegun_snapshot.forward;
		const mat4 weapon_rotation = rotation_matrix
			* glm::scale(mat4(1.0f), vec3(machinegun_snapshot.scale));
		const glm::dvec3 muzzle_position = world.point_data.getPoint(slot.point_id).body_local_position
			+ glm::dvec3(weapon_rotation * vec4(
			muzzle_forward_offset,
			muzzle_vertical_offset,
			0.0f,
			0.0f));

		for(uint32 shot_index = 0; shot_index < shot_count; shot_index++)
		{
			spawnMachinegunBullet(body_id, muzzle_position, direction, world);
		}
	}

	/**
	 * @brief Draws the shared MG-42 item mesh with the component's local basis and scale.
	 * @param slot Slot containing payload, attached point ID, and owner body ID.
	 * @param world World used to build the body's world matrix.
	 */
	void frameMachinegunComponent(PointComponentSlot& slot, World& world)
	{
		DSE_WATCH();

		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component frame received null data");
		}

		if(!world.point_data.isPointActive(slot.point_id))
		{
			danikk_engine::formatFatalError("Machinegun component references invalid point %", slot.point_id);
		}
		const MachinegunComponent& machinegun = *static_cast<const MachinegunComponent*>(slot.getData());
		const mat4 world_matrix = world.getPointWorldMatrix(slot.point_id)
			* getOrientationMatrix(machinegun)
			* glm::scale(mat4(1.0f), vec3(machinegun.scale));
		drawItem(item_definition_ids::mg42, world_matrix);
	}
}
