#include <utility>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/unit_system.h>
#include <danikk_space_engine/asset.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	/** @copydoc spawnBullet */
	void spawnBullet(World& world, ChunkId chunk_id, const vec3& position, const BulletComponent& bullet)
	{
		const PointId point_id = world.data.createPoint();
		world.data.getPoint(point_id).position = position;
		BulletComponent initial_bullet = bullet;
		initial_bullet.previous_position = position;
		const PointComponentSlotId slot_id = world.data.createPointComponentSlot(point_id, PointComponentTypeId::bullet, &initial_bullet);
		world.chunk_system.addPointComponentSlotToChunk(world, chunk_id, slot_id);
	}

	/**
	 * @brief Constructs default projectile state or copies the caller-provided velocity/lifetime.
	 * @param destination Destination storage owned by the point-component slot.
	 * @param init_data Optional initial `BulletComponent`; null selects the default velocity and lifetime.
	 */
	void constructBulletComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component construction received null destination");
		}
		if(init_data == nullptr)
		{
			new (destination) BulletComponent();
		}
		else
		{
			new (destination) BulletComponent(*static_cast<const BulletComponent*>(init_data));
		}
	}

	/**
	 * @brief Transfers projectile state to replacement slot storage and destroys the source payload.
	 * @param destination Replacement payload storage.
	 * @param source Existing live projectile payload.
	 */
	void moveBulletComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component move received null memory");
		}
		BulletComponent* source_component = static_cast<BulletComponent*>(source);
		new (destination) BulletComponent(std::move(*source_component));
		std::destroy_at(source_component);
	}

	/**
	 * @brief Ends the projectile payload lifetime; the slot pool manages backing storage.
	 * @param data Existing live projectile payload.
	 */
	void destroyBulletComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component destruction received null memory");
		}
		std::destroy_at(static_cast<BulletComponent*>(data));
	}

	/**
	 * @brief Sweeps the projectile segment for impact, advances lifetime, and queues point removal.
	 * @param slot Slot containing projectile state and point/chunk IDs.
	 * @param world World used for world-space raycasting and deferred point destruction.
	 * Uses the target frame delay and raycasts the full displacement before moving
	 * the point, preventing a fast projectile from skipping a surface between ticks.
	 * On impact the point is moved to the hit position and destruction is deferred;
	 * otherwise age advances and an expired projectile is queued for removal.
	 */
	void tickBulletComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();

		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component tick received null data");
		}
		BulletComponent& bullet = *static_cast<BulletComponent*>(slot.getData());
		if(bullet.destruction_queued) return;

		const float delta_time = getTargetFrameDelay();
		if(delta_time <= 0.0f) return;
		Point& point = world.data.getPoint(slot.point_id);
		const vec3 start_position = point.position;
		bullet.previous_position = start_position;
		const vec3 displacement = bullet.velocity * delta_time;
		const float step_distance = glm::length(displacement);
		if(step_distance > 0.000001f)
		{
			const glm::dvec3 start_world_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			const Chunk& owner_chunk = world.chunk_system.getChunk(slot.chunk_id);
			const glm::dquat body_rotation = world.data.getWorldBody(owner_chunk.world_body_id).rotation;
			const glm::dvec3 direction = glm::normalize(body_rotation * glm::dvec3(bullet.velocity));
			const RaycastHit hit = world.raycast(start_world_position, vec3(direction), step_distance);
			double nearest = hit.hit ? hit.distance : step_distance;
			PointComponentSlotId victim_id = static_cast<PointComponentSlotId>(-1);
			PointComponentSlotId suppression_target_id = static_cast<PointComponentSlotId>(-1);
			double nearest_suppression_distance = nearest;
			for(PointComponentSlotId id : world.data.getUsedPointComponentSlotIds())
			{
				if(!world.data.isPointComponentSlotActive(id)) continue;
				const PointComponentSlot& candidate = world.data.getPointComponentSlot(id);
				if(candidate.type_id != PointComponentTypeId::human || candidate.point_id == bullet.owner_point_id
					|| !world.data.isPointActive(candidate.point_id) || !world.chunk_system.isChunkActive(candidate.chunk_id)) continue;
				const HumanComponent& human = *static_cast<const HumanComponent*>(candidate.getData());
				const DamageableComponent* health = findDamageable(world, candidate.chunk_id, candidate.point_id);
				if(health == nullptr || health->getHealth() <= 0.0f) continue;
				const Chunk& human_chunk = world.chunk_system.getChunk(candidate.chunk_id);
				const glm::dvec3 up = glm::normalize(world.data.getWorldBody(human_chunk.world_body_id).rotation * glm::dvec3(human.up));
				const glm::dvec3 center = world.getPointWorldPosition(candidate.chunk_id, candidate.point_id);
				const double radius = human.getHeight() * 0.14;
				for(int i = -2; i <= 2; ++i)
				{
					const glm::dvec3 offset = center + up * (i * human.getHeight() * 0.18) - start_world_position;
					const double along = glm::dot(offset, direction);
					const double perpendicular = glm::dot(offset, offset) - along * along;
					const double suppression_radius = radius + 0.75;
					if(along >= 0.0 && along <= nearest && perpendicular <= suppression_radius * suppression_radius
						&& along < nearest_suppression_distance)
					{
						nearest_suppression_distance = along;
						suppression_target_id = id;
					}
					if(perpendicular > radius * radius) continue;
					const double half_chord = std::sqrt(glm::max(0.0, radius * radius - perpendicular));
					if(along + half_chord < 0.0) continue;
					const double entry = glm::max(0.0, along - half_chord);
					if(entry < nearest) { nearest = entry; victim_id = id; }
				}
			}
			if(victim_id != static_cast<PointComponentSlotId>(-1))
			{
				const PointComponentSlot& victim = world.data.getPointComponentSlot(victim_id);
				DamageableComponent* health = findDamageable(world, victim.chunk_id, victim.point_id);
				if(health != nullptr)
				{
					reportUnitAttack(world, bullet.owner_point_id, victim.point_id);
					applyBallisticDamage(*health, bullet.damage, victim.point_id);
				}
				bullet.destruction_queued = true;
				world.deferPointDestruction(slot.point_id);
				return;
			}
			if(suppression_target_id != static_cast<PointComponentSlotId>(-1))
			{
				const PointComponentSlot& target = world.data.getPointComponentSlot(suppression_target_id);
				reportUnitAttack(world, bullet.owner_point_id, target.point_id);
			}
			if(hit.hit)
			{
				if(hit.hit_build_part && world.data.isPointComponentSlotActive(hit.build_part_slot_id))
				{
					const PointComponentSlot& part = world.data.getPointComponentSlot(hit.build_part_slot_id);
					DamageableComponent* health = findDamageable(world, part.chunk_id, part.point_id);
					if(health != nullptr)
					{
						applyBallisticDamage(*health, bullet.damage, part.point_id);
						if(health->getHealth() <= 0.0f) world.deferPointDestruction(part.point_id);
					}
				}
				point.position = vec3(glm::conjugate(body_rotation) * (hit.position - world.getChunkWorldPosition(slot.chunk_id)));
				bullet.destruction_queued = true;
				world.deferPointDestruction(slot.point_id);
				return;
			}
			point.position = start_position + displacement;
		}

		bullet.age += delta_time;
		if(bullet.age >= bullet.lifetime)
		{
			bullet.destruction_queued = true;
			world.deferPointDestruction(slot.point_id);
		}
	}

	/**
	 * @brief Renders the travelled projectile segment as a narrow quad facing the camera.
	 * @param slot Slot containing projectile state and render transform IDs.
	 * @param world World used to convert the camera position into chunk-local space.
	 * The quad spans the previous and current projectile positions. Its lateral axis is
	 * perpendicular to both the segment and the view direction, with a fallback axis
	 * for near-parallel vectors. A shared dynamic mesh is updated and drawn immediately.
	 */
	void frameBulletComponent(PointComponentSlot& slot, World& world)
	{
		DSE_WATCH();

		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component frame received null data");
		}

		const BulletComponent& bullet = *static_cast<const BulletComponent*>(slot.getData());
		if(bullet.destruction_queued) return;
		const vec3 current_position = world.data.getPoint(slot.point_id).position;
		const float velocity_length = glm::length(bullet.velocity);
		if(velocity_length <= 0.000001f) return;
		const vec3 forward = bullet.velocity / velocity_length;
		const float actual_segment_length = glm::length(current_position - bullet.previous_position);
		const float visible_trail_length = glm::min(velocity_length * bullet.age,
			glm::max(actual_segment_length, glm::min(velocity_length * 0.12f, 12.0f)));
		const vec3 start_position = bullet.previous_position
			- forward * glm::max(0.0f, visible_trail_length - actual_segment_length);
		const Chunk& chunk = world.chunk_system.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dvec3 camera_local_position = glm::inverse(body.rotation)
			* (world.getCameraWorldPosition() - body.position) - chunk.position;
		const vec3 midpoint = (start_position + current_position) * 0.5f;
		const vec3 to_camera = vec3(camera_local_position - glm::dvec3(midpoint));
		vec3 side = glm::cross(forward, to_camera);
		if(glm::dot(side, side) <= 1e-10f)
		{
			const vec3 fallback_axis = glm::abs(forward.z) < 0.9f
				? vec3(0.0f, 0.0f, 1.0f)
				: vec3(0.0f, 1.0f, 0.0f);
			side = glm::cross(forward, fallback_axis);
		}
		side = glm::normalize(side) * 0.025f;
		vec3 normal = glm::normalize(glm::cross(forward, side));
		if(glm::dot(normal, to_camera) < 0.0f)
		{
			side = -side;
			normal = -normal;
		}
		const vec3 start_left = start_position - side;
		const vec3 end_left = current_position - side;
		const vec3 end_right = current_position + side;
		const vec3 start_right = start_position + side;
		const DefaultVertex vertices[4] =
		{
			{start_left, normal, vec2(0.0f, 0.0f)},
			{end_left, normal, vec2(1.0f, 0.0f)},
			{end_right, normal, vec2(1.0f, 1.0f)},
			{start_right, normal, vec2(0.0f, 1.0f)}
		};
		const gl_point_index_t indices[6] = {0, 1, 2, 0, 2, 3};
		static Mesh tracer_mesh;
		tracer_mesh.setData(reinterpret_cast<const float*>(vertices),
			4 * (sizeof(DefaultVertex) / sizeof(float)), indices, 6);
		DefaultVertex::setAttributes();
		static_asset_collection.bullet_tracer_shader.use();
		const CameraComponent& camera = world.getCameraComponent();
		const mat4 view = glm::lookAt(vec3(0.0f), camera.getFront(), camera.getUp());
		const mat4 projection = glm::perspective(glm::radians(camera.vertical_fov_degrees),
			screen_ratio_gz, camera.near_clip_distance, camera.far_clip_distance);
		setViewMatrix(view);
		setProjectionMatrix(projection);
		white_texture.bind();
		setDrawColor(vec4(1.0f, 1.0f, 0.0f, 1.0f));
		setWorldMatrix(chunk.createWorldMatrix(world));
		tracer_mesh.draw();
		static_asset_collection.base3d_shader.use(true);
	}
}
