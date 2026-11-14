#include <utility>

#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
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

	void destroyBulletComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Bullet component destruction received null memory");
		}
		std::destroy_at(static_cast<BulletComponent*>(data));
	}

	// Advances the projectile over this simulation step, raycasts the swept segment to prevent
	// tunneling through world cells, and queues the point for cleanup on impact or timeout.
	void tickBulletComponent(PointComponentSlot& slot, World& world)
	{
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

	// Builds a camera-independent orientation from the velocity and draws a narrow, bright sphere
	// along the travel axis so the fast-moving projectile remains visible as a short tracer.
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
