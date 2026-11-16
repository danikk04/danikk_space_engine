#include <utility>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{

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
		Watcher watcher("BulletComponent::tick");

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
		const vec3 displacement = bullet.velocity * delta_time;
		const float step_distance = glm::length(displacement);
		if(step_distance > 0.000001f)
		{
			const glm::dvec3 start_world_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
			const RaycastHit hit = world.raycast(start_world_position, bullet.velocity, step_distance);
			if(hit.hit)
			{
				point.position = vec3(hit.position - world.getChunkWorldPosition(slot.chunk_id));
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
	 * @brief Renders an orange tracer aligned to projectile velocity using the shared sphere mesh.
	 * @param slot Slot containing projectile state and render transform IDs.
	 * @param world World used to construct the camera-relative chunk transform.
	 * The tracer is assembled from a scaled low-poly sphere mesh, oriented along
	 * velocity and drawn with a bright solid tint. Projectiles already queued for
	 * destruction are skipped; missing shared mesh state is fatal.
	 */
	void frameBulletComponent(PointComponentSlot& slot, World& world)
	{

		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component frame received null data");
		}

		if(static_asset_collection.low_poly_sphere_mesh.isNull())
		{
			danikk_engine::formatFatalError("Bullet component has no loaded sphere mesh");
		}

		const BulletComponent& bullet = *static_cast<const BulletComponent*>(slot.getData());
		if(bullet.destruction_queued) return;
		const vec3 position = world.data.getPoint(slot.point_id).position;
		const float velocity_length = glm::length(bullet.velocity);
		const vec3 forward = velocity_length > 0.000001f
			? bullet.velocity / velocity_length
			: vec3(1.0f, 0.0f, 0.0f);
		const vec3 reference_axis = glm::abs(forward.y) < 0.95f
			? vec3(0.0f, 1.0f, 0.0f)
			: vec3(0.0f, 0.0f, 1.0f);
		const vec3 side = glm::normalize(glm::cross(reference_axis, forward));
		const vec3 up = glm::cross(forward, side);
		mat4 orientation(1.0f);
		orientation[0] = vec4(forward, 0.0f);
		orientation[1] = vec4(side, 0.0f);
		orientation[2] = vec4(up, 0.0f);

		white_texture.bind();
		setDrawColor(vec4(1.0f, 0.78f, 0.28f, 1.0f));
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		setWorldMatrix(
			chunk.createWorldMatrix(world)
			* glm::translate(mat4(1.0f), position)
			* orientation
			* glm::scale(mat4(1.0f), vec3(0.18f, 0.025f, 0.025f)));
		static_asset_collection.low_poly_sphere_mesh.draw();
	}
}
