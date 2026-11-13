#include <utility>

#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr float bullet_interval = 0.25f;
		constexpr float bullet_speed = 120.0f;
		constexpr float muzzle_forward_offset = 4.4f;
		constexpr float muzzle_vertical_offset = 0.31f;

		ChunkId findOwningChunkId(PointComponentSlot& slot, World& world)
		{
			for(ChunkId chunk_id : world.data.getUsedChunkIds())
			{
				const Chunk& chunk = world.data.getChunk(chunk_id);
				for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
				{
					if(world.data.isPointComponentSlotActive(slot_id)
						&& &world.data.getPointComponentSlot(slot_id) == &slot)
					{
						return chunk_id;
					}
				}
			}
			return static_cast<ChunkId>(-1);
		}

		void spawnBullet(ChunkId chunk_id, const vec3& position, const vec3& direction, World& world)
		{
			const PointId point_id = world.data.createPoint();
			world.data.getPoint(point_id).position = position;
			BulletComponent bullet;
			bullet.velocity = direction * bullet_speed;
			const PointComponentSlotId bullet_slot_id = world.data.createPointComponentSlot(
				point_id,
				PointComponentTypeId::bullet,
				&bullet);
			world.data.getChunk(chunk_id).addPointComponentSlot(bullet_slot_id);
		}
	}

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

	void destroyMachinegunComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component destruction received null memory");
		}
		std::destroy_at(static_cast<MachinegunComponent*>(data));
	}

	// Accumulates simulation time, resolves the weapon's owning chunk and muzzle transform,
	// then creates one moving bullet point for every elapsed 0.25-second firing interval.
	void tickMachinegunComponent(PointComponentSlot& slot, World& world)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component tick received null data");
		}
		if(!world.data.isPointActive(slot.point_id))
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
		const mat4 rotation_matrix = glm::rotate(mat4(1.0f), -machinegun_snapshot.rotation.x, vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(mat4(1.0f), machinegun_snapshot.rotation.y, vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(mat4(1.0f), machinegun_snapshot.rotation.z, vec3(1.0f, 0.0f, 0.0f));
		const vec3 direction = glm::normalize(vec3(rotation_matrix * vec4(1.0f, 0.0f, 0.0f, 0.0f)));
		const mat4 weapon_matrix = glm::translate(mat4(1.0f), world.data.getPoint(slot.point_id).position)
			* rotation_matrix
			* glm::scale(mat4(1.0f), vec3(machinegun_snapshot.scale));
		const vec3 muzzle_position = vec3(weapon_matrix * vec4(
			muzzle_forward_offset,
			muzzle_vertical_offset,
			0.0f,
			1.0f));

		for(uint32 shot_index = 0; shot_index < shot_count; shot_index++)
		{
			spawnBullet(chunk_id, muzzle_position, direction, world);
		}
	}

	// Draws the shared MG-42 mesh at the position of the point that owns this component.
	// The component scale is applied after translation; OBJ groups and materials have already
	// been combined into the shared mesh during static asset loading.
	void frameMachinegunComponent(PointComponentSlot& slot, World& world)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component frame received null data");
		}
		if(!world.data.isPointActive(slot.point_id))
		{
			danikk_engine::formatFatalError("Machinegun component references invalid point %", slot.point_id);
		}
		if(static_asset_collection.mg42_mesh.isNull())
		{
			danikk_engine::formatFatalError("Machinegun component has no loaded MG-42 mesh");
		}

		const MachinegunComponent& machinegun = *static_cast<const MachinegunComponent*>(slot.getData());
		white_texture.bind();
		setDrawColor(vec4(0.4f, 0.4f, 0.4f, 1.0f));
		const mat4 world_matrix = glm::translate(
			mat4(1.0f),
			world.data.getPoint(slot.point_id).position)
			* glm::rotate(mat4(1.0f), -machinegun.rotation.x, vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(mat4(1.0f), machinegun.rotation.y, vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(mat4(1.0f), machinegun.rotation.z, vec3(1.0f, 0.0f, 0.0f))
			* glm::scale(mat4(1.0f), vec3(machinegun.scale));
		setWorldMatrix(world_matrix);
		static_asset_collection.mg42_mesh.draw();
	}
}
