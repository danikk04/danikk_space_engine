#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine
{
	using human_internal::moveWithGround;
	using human_internal::drawEquippedWeapons;


	/**
	 * @brief Creates default human state or copies the provided initial state into slot storage.
	 * @param destination Destination storage owned by WorldData.
	 * @param init_data Optional source `HumanComponent`; null initializes member defaults.
	 */
	void constructHumanComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr) formatFatalError("Human construction received null destination");
		new (destination) HumanComponent(init_data == nullptr
			? HumanComponent() : *static_cast<const HumanComponent*>(init_data));
	}

	/**
	 * @brief Copies human state into relocated storage and ends the source payload lifetime.
	 * @param destination Replacement payload storage.
	 * @param source Existing live human payload.
	 */
	void moveHumanComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr) formatFatalError("Human move received null memory");
		new (destination) HumanComponent(*static_cast<HumanComponent*>(source));
		static_cast<HumanComponent*>(source)->~HumanComponent();
	}

	/**
	 * @brief Ends the human payload lifetime; WorldData owns release of the backing storage.
	 * @param data Existing live human payload.
	 */
	void destroyHumanComponent(void* data)
	{
		if(data == nullptr) formatFatalError("Human destruction received null data");
		static_cast<HumanComponent*>(data)->~HumanComponent();
	}

	/**
	 * @brief Reorients local up from gravity, consumes requested movement, and updates locomotion state.
	 *
	 * The movement command is cleared after reading. Ground following delegates to support tests;
	 * walking phase and facing are advanced from actual tangential displacement.
	 * @param slot Human component slot containing point and chunk ownership.
	 * @param world World supplying gravity samples and collision/support queries.
	 */
	void tickHumanComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("HumanComponent::tick");
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id))
			formatFatalError("Human references an invalid point or chunk");
		HumanComponent& human = *static_cast<HumanComponent*>(slot.getData());
		Point& point = world.data.getPoint(slot.point_id);
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dvec3 world_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		const glm::dvec3 gravity_world = world.environment.sampleGravityAt(world, world_position);
		const double gravity_magnitude = glm::length(gravity_world);
		const float gravity = static_cast<float>(gravity_magnitude);
		if(gravity_magnitude > 1e-8)
		{
			human.up = vec3(glm::conjugate(body.rotation) * (-gravity_world / gravity_magnitude));
		}
		if(glm::length(human.up) < 1e-6f) human.up = vec3(0, 0, 1);
		human.up = glm::normalize(human.up);
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		const DamageableComponent* damageable = findDamageable(world, slot.chunk_id, slot.point_id);
		if(damageable != nullptr && damageable->getHealth() <= 0.0f)
		{
			human.death_time += dt;
			human.desired_velocity = vec3(0.0f);
			human.walk_amplitude = 0.0f;
			if(human.death_time >= 1.0f) world.deferPointDestruction(slot.point_id);
			return;
		}
		human.death_time = 0.0f;
		vec3 velocity = damageable == nullptr || damageable->getHealth() > 0.0f ? human.desired_velocity : vec3(0.0f);
		human.desired_velocity = vec3(0.0f);
		velocity -= human.up * glm::dot(velocity, human.up);
		const float speed = glm::length(velocity);
		const float max_speed = glm::max(human.getMovementSpeed(), 0.0f);
		if(speed > max_speed && speed > 0.0f) velocity *= max_speed / speed;
		const vec3 old_position = point.position;
		moveWithGround(world, chunk, body, human, point, velocity, gravity, dt);
		vec3 displacement = point.position - old_position;
		displacement -= human.up * glm::dot(displacement, human.up);
		const float distance = glm::length(displacement);
		human.updateOrientation(displacement, dt);
		human.walk_phase = std::fmod(human.walk_phase + distance * 6.0f, 6.283185307f);
		const float actual_speed = dt > 0.0f ? distance / dt : 0.0f;
		const float target_amplitude = max_speed > 0.0f ? glm::min(actual_speed / max_speed, 1.0f) * 0.6f : 0.0f;
		human.walk_amplitude = glm::mix(human.walk_amplitude, target_amplitude, glm::min(dt * 10.0f, 1.0f));
	}

	/**
	 * @brief Builds the character transform from its center point and renders body plus equipment.
	 *
	 * The visible model root is shifted down by half the character height, while the point remains
	 * at the body's vertical center. Rendering is camera-relative and uses the body's rotation.
	 * @param slot Human component slot, containing point and owning chunk IDs.
	 * @param world World used for world transforms, camera origin, and equipped inventory lookup.
	 */
	void frameHumanComponent(PointComponentSlot& slot, World& world)
	{
		if(!world.data.isPointActive(slot.point_id) || !world.data.isChunkActive(slot.chunk_id)) return;
		const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const vec3 up = vec3(body.rotation * glm::dvec3(human.up));
		const vec3 forward = vec3(body.rotation * glm::dvec3(human.getForward()));
		const vec3 right = glm::normalize(glm::cross(forward, up));
		mat4 orientation(1.0f);
		orientation[0] = vec4(right, 0);
		orientation[1] = vec4(forward, 0);
		orientation[2] = vec4(up, 0);
		const glm::dvec3 position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		mat4 body_matrix = glm::translate(mat4(1.0f), vec3(position - world.getCameraWorldPosition()))
			* orientation;
		if(human.death_time > 0.0f)
		{
			const float fall_angle = glm::radians(90.0f) * glm::clamp(human.death_time / 0.6f, 0.0f, 1.0f);
			const vec3 pivot(0.0f, 0.0f, -human.getHeight() * 0.5f);
			body_matrix = body_matrix * glm::translate(mat4(1.0f), pivot)
				* glm::rotate(mat4(1.0f), fall_angle, vec3(0.0f, 1.0f, 0.0f))
				* glm::translate(mat4(1.0f), -pivot);
		}
		const mat4 feet_matrix = body_matrix
			* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
			* glm::scale(mat4(1.0f), vec3(human.getHeight()));
		drawHumanAppearance(human, feet_matrix);
		drawEquippedWeapons(slot, human, body_matrix, world);
	}
}
