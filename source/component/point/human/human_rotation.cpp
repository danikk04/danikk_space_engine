#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
	void HumanComponent::setInitialForward(const vec3& direction)
	{
		DSE_WATCH();
		vec3 horizontal = direction - up * glm::dot(direction, up);
		if(glm::length(horizontal) < 1e-6f)
		{
			const vec3 reference = std::abs(up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			horizontal = reference - up * glm::dot(reference, up);
		}
		forward = glm::normalize(horizontal);
	}

	void HumanComponent::lookAtDirection(const vec3& direction, float target_distance)
	{
		DSE_WATCH();
		if(glm::length(direction) > 1e-6f)
		{
			target_look_direction = glm::normalize(direction);
			target_look_distance = glm::max(target_distance, 0.0f);
		}
	}

	vec3 HumanComponent::getWeaponDirection() const
	{
		DSE_WATCH();
		return glm::normalize(forward * std::cos(weapon_aim_pitch) + up * std::sin(weapon_aim_pitch));
	}

	void HumanComponent::updateOrientation(const vec3& displacement, float dt)
	{
		DSE_WATCH();
		setInitialForward(forward);
		const bool looking = glm::length(target_look_direction) > 1e-6f;
		const vec3 requested = looking ? target_look_direction : displacement;
		const vec3 horizontal = requested - up * glm::dot(requested, up);
		if(glm::length(horizontal) > 1e-6f)
		{
			const vec3 heading = glm::normalize(horizontal);
			const float angle = std::atan2(glm::dot(up, glm::cross(forward, heading)),
				glm::clamp(glm::dot(forward, heading), -1.0f, 1.0f));
			forward = glm::angleAxis(glm::clamp(angle, -dt * 4.0f, dt * 4.0f), up) * forward;
		}
		const float target_head_pitch = looking
			? glm::clamp(std::atan2(glm::dot(requested, up), glm::length(horizontal)), -0.65f, 0.65f)
			: 0.0f;
		float target_weapon_pitch = target_head_pitch;
		if(looking && target_look_distance > 1e-3f)
		{
			const vec3 target_from_point = up * (getHeight() * 0.375f) + requested * target_look_distance;
			const vec3 muzzle_from_point = up * (getHeight() * 0.2f) + forward * (getHeight() * 0.6f);
			const vec3 muzzle_to_target = target_from_point - muzzle_from_point;
			const float weapon_horizontal_distance = glm::length(
				muzzle_to_target - up * glm::dot(muzzle_to_target, up));
			target_weapon_pitch = glm::clamp(
				std::atan2(glm::dot(muzzle_to_target, up), weapon_horizontal_distance), -0.8f, 0.8f);
		}
		else if(!looking)
		{
			target_weapon_pitch = 0.0f;
		}
		head_pitch += glm::clamp(target_head_pitch - head_pitch, -dt * 4.0f, dt * 4.0f);
		weapon_aim_pitch += glm::clamp(target_weapon_pitch - weapon_aim_pitch, -dt * 4.0f, dt * 4.0f);
		target_look_direction = vec3(0.0f);
		target_look_distance = 0.0f;
	}

}
