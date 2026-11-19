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
		 * @brief Searches chunk component lists for the chunk that owns a given weapon slot.
		 * @param slot Component slot whose address is matched against chunk membership.
		 * @param world World whose active bodies and chunks are searched.
		 * @return Owning chunk ID, or the invalid sentinel if the slot is not registered.
		 */
		ChunkId findOwningChunkId(PointComponentSlot& slot, World& world)
		{
			for(WorldBodyId body_id : world.world_body_data.getUsedWorldBodyIds())
			{
				const WorldBody& body = world.world_body_data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					const Chunk& chunk = world.chunks.getChunk(chunk_id);
					for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
					{
						if(world.point_data.isPointComponentSlotActive(slot_id)
							&& &world.point_data.getPointComponentSlot(slot_id) == &slot) return chunk_id;
					}
				}
			}
			return static_cast<ChunkId>(-1);
		}

		/**
		 * @brief Creates a point and bullet component at a muzzle position in the owner chunk.
		 * @param chunk_id Chunk that will own the new projectile component.
		 * @param position Muzzle position in chunk-local coordinates.
		 * @param direction Unit travel direction inherited from the weapon barrel.
		 * @param world World used to allocate and register the projectile resources.
		 */
		void spawnMachinegunBullet(ChunkId chunk_id, const vec3& position, const vec3& direction, World& world)
		{
			const WeaponItemParameters& weapon = getWeaponItemParameters(item_definition_ids::mg42);
			const ProjectileDefinition& projectile = getProjectileDefinition(weapon.projectile_id);
			BulletComponent bullet;
			bullet.velocity = applyAngularSpread(direction, weapon.spread_degrees) * projectile.speed;
			bullet.lifetime = projectile.lifetime;
			bullet.damage = weapon.damage;
			spawnBullet(world, chunk_id, position, bullet);
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
	 * @param world World used to find the owning chunk and allocate projectile resources.
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

		const ChunkId chunk_id = findOwningChunkId(slot, world);
		if(chunk_id == static_cast<ChunkId>(-1))
		{
			danikk_engine::formatFatalError("Machinegun component is not registered in a chunk");
		}

		const MachinegunComponent machinegun_snapshot = machinegun;
		const mat4 rotation_matrix = getOrientationMatrix(machinegun_snapshot);
		const vec3 direction = machinegun_snapshot.forward;
		const mat4 weapon_matrix = glm::translate(mat4(1.0f), world.point_data.getPoint(slot.point_id).position)
			* rotation_matrix
			* glm::scale(mat4(1.0f), vec3(machinegun_snapshot.scale));
		const vec3 muzzle_position = vec3(weapon_matrix * vec4(
			muzzle_forward_offset,
			muzzle_vertical_offset,
			0.0f,
			1.0f));

		for(uint32 shot_index = 0; shot_index < shot_count; shot_index++)
		{
			spawnMachinegunBullet(chunk_id, muzzle_position, direction, world);
		}
	}

	/**
	 * @brief Draws the shared MG-42 item mesh with the component's local basis and scale.
	 * @param slot Slot containing payload, attached point ID, and owner chunk ID.
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
		const Chunk& chunk = world.chunks.getChunk(slot.chunk_id);
		const mat4 world_matrix = chunk.createWorldMatrix(world)
			* glm::translate(
				mat4(1.0f),
				world.point_data.getPoint(slot.point_id).position)
			* getOrientationMatrix(machinegun)
			* glm::scale(mat4(1.0f), vec3(machinegun.scale));
		drawItem(item_definition_ids::mg42, world_matrix);
	}
}
