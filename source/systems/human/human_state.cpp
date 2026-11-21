#include <cmath>

#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/random.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/systems/human/data.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/world.h>

#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
	}

	/** @brief Stores the death timestamp only once, preserving the original death moment. */
	void HumanState::startDeathTimestamp(uint32 timestamp_ms)
	{
		if(death_timestamp_ms != 0) return;
		death_timestamp_ms = timestamp_ms;
	}

	/** @brief Resets death state when health is restored. */
	void HumanState::clearDeathTimestamp()
	{
		death_timestamp_ms = 0;
	}

	/** @brief Computes elapsed death time with modulo-safe subtraction across uint32 wraparound. */
	uint32 HumanState::getDeathElapsedMilliseconds() const
	{
		return death_timestamp_ms != 0 ? danikk_engine::getMsTimestamp() - death_timestamp_ms : 0;
	}

	using human_internal::moveWithGround;
	using human_internal::drawEquippedWeapons;


	void tickHumanData(HumanData& human, World& world)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(human.world_body_id))
			formatFatalError("Human % references an invalid WorldBody", human.id);
		WorldBody& body = world.world_body_data.getWorldBody(human.world_body_id);
		const glm::dvec3 world_position = world.humans.getWorldPosition(human);
		const glm::dvec3 gravity_world = world.environment.sampleGravityAt(world_position);
		const double gravity_magnitude = glm::length(gravity_world);
		const float gravity = static_cast<float>(gravity_magnitude);
		if(gravity_magnitude > 1e-8)
			human.up = vec3(glm::conjugate(body.rotation) * (-gravity_world / gravity_magnitude));
		if(glm::length(human.up) < 1e-6f) human.up = vec3(0, 0, 1);
		human.up = glm::normalize(human.up);
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		tickInventory(human.inventory, dt);
		if(human.damageable.getHealth() <= 0.0f)
		{
			if(human.body_pose == HumanBodyPose::standing)
			{
				human.body_pose = danikk_framework::default_random.number<uint32>(0, 2) == 0
					? HumanBodyPose::lying_on_back : HumanBodyPose::lying_on_stomach;
				for(InventorySlot& item : human.inventory.slots)
					if(!item.isEmpty() && item.is_equipped) item.is_equipped = false;
				const float random_offset = 0.5f;
				human.corpse_arm_spread_angles = vec2(
					glm::radians(60.0f + (danikk_framework::default_random.nextFloat() * random_offset + random_offset) * 30.0f),
					glm::radians(60.0f + (danikk_framework::default_random.nextFloat() * random_offset + random_offset) * 30.0f));
				human.arm_pose = HumanArmPose::corpse_spread;
			}
			human.startDeathTimestamp(danikk_engine::getMsTimestamp());
			human.desired_velocity = vec3(0.0f);
			human.walk_amplitude = 0.0f;
			using human_internal::moveWithGround;
			moveWithGround(world, human.world_body_id, body, human, human.body_local_position,
				vec3(0.0f), gravity, dt);
			return;
		}
		human.clearDeathTimestamp();
		if(human.body_pose != HumanBodyPose::standing)
		{
			human.arm_pose = HumanArmPose::relaxed;
			human.corpse_arm_spread_angles = vec2(0.0f);
		}
		human.body_pose = HumanBodyPose::standing;
		vec3 velocity = human.desired_velocity;
		human.desired_velocity = vec3(0.0f);
		velocity -= human.up * glm::dot(velocity, human.up);
		const float speed = glm::length(velocity);
		const float max_speed = glm::max(human.getMovementSpeed(), 0.0f);
		if(speed > max_speed && speed > 0.0f) velocity *= max_speed / speed;
		const glm::dvec3 old_position = human.body_local_position;
		using human_internal::moveWithGround;
		moveWithGround(world, human.world_body_id, body, human, human.body_local_position,
			velocity, gravity, dt);
		vec3 displacement = vec3(human.body_local_position - old_position);
		displacement -= human.up * glm::dot(displacement, human.up);
		const float distance = glm::length(displacement);
		human.updateOrientation(displacement, dt);
		human.walk_phase = std::fmod(human.walk_phase + distance * 6.0f, 6.283185307f);
		const float actual_speed = dt > 0.0f ? distance / dt : 0.0f;
		const float target_amplitude = max_speed > 0.0f ? glm::min(actual_speed / max_speed, 1.0f) * 0.6f : 0.0f;
		human.walk_amplitude = glm::mix(human.walk_amplitude, target_amplitude, glm::min(dt * 10.0f, 1.0f));
	}

	void frameHumanData(const HumanData& human, World& world)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(human.world_body_id)) return;
		const WorldBody& body = world.world_body_data.getWorldBody(human.world_body_id);
		vec3 local_up = human.body_pose == HumanBodyPose::standing ? human.up : human.ground_normal;
		if(glm::length(local_up) <= 1e-6f) local_up = human.up;
		local_up = glm::normalize(local_up);
		vec3 local_forward = human.getForward() - local_up * glm::dot(human.getForward(), local_up);
		if(glm::length(local_forward) <= 1e-6f)
		{
			local_forward = vec3(0.0f, 1.0f, 0.0f)
				- local_up * glm::dot(vec3(0.0f, 1.0f, 0.0f), local_up);
			if(glm::length(local_forward) <= 1e-6f)
				local_forward = vec3(1.0f, 0.0f, 0.0f)
					- local_up * glm::dot(vec3(1.0f, 0.0f, 0.0f), local_up);
		}
		local_forward = glm::normalize(local_forward);
		const vec3 local_right = glm::normalize(glm::cross(local_forward, local_up));
		local_forward = glm::normalize(glm::cross(local_up, local_right));
		const vec3 up = vec3(body.rotation * glm::dvec3(local_up));
		const vec3 forward = vec3(body.rotation * glm::dvec3(local_forward));
		const vec3 right = vec3(body.rotation * glm::dvec3(local_right));
		mat4 orientation(1.0f);
		orientation[0] = vec4(right, 0);
		orientation[1] = vec4(forward, 0);
		orientation[2] = vec4(up, 0);
		const glm::dvec3 position = world.humans.getWorldPosition(human);
		mat4 body_matrix = glm::translate(mat4(1.0f), vec3(position - world.camera.getWorldPosition()))
			* orientation;
		if(human.body_pose != HumanBodyPose::standing)
		{
			const float angle = human.body_pose == HumanBodyPose::lying_on_back
				? glm::radians(90.0f) : glm::radians(-90.0f);
			body_matrix = body_matrix
				* glm::rotate(mat4(1.0f), angle, vec3(1.0f, 0.0f, 0.0f));
		}
		const mat4 feet_matrix = body_matrix
			* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
			* glm::scale(mat4(1.0f), vec3(human.getHeight()));
		drawHumanAppearance(human, feet_matrix);
		human_internal::drawEquippedWeapons(human.inventory, human, body_matrix);
	}
}
