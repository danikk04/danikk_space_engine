#include <cmath>

#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
	void HumanComponent::setInitialForward(const vec3& direction)
	{
		vec3 horizontal = direction - up * glm::dot(direction, up);
		if(glm::length(horizontal) < 1e-6f)
		{
			const vec3 reference = std::abs(up.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
			horizontal = reference - up * glm::dot(reference, up);
		}
		forward = glm::normalize(horizontal);
	}

	void HumanComponent::lookAtDirection(const vec3& direction)
	{
		if(glm::length(direction) > 1e-6f) target_look_direction = glm::normalize(direction);
	}

	vec3 HumanComponent::getWeaponDirection() const
	{
		const vec3 facing = glm::angleAxis(head_rotation.x, up) * forward;
		return glm::normalize(facing * std::cos(head_rotation.y) + up * std::sin(head_rotation.y));
	}

	void HumanComponent::updateOrientation(const vec3& displacement, float dt)
	{
		setInitialForward(forward);
		const bool looking = glm::length(target_look_direction) > 1e-6f;
		const vec3 requested = looking ? target_look_direction : displacement;
		const vec3 horizontal = requested - up * glm::dot(requested, up);
		if(glm::length(horizontal) > 1e-6f)
		{
			const vec3 heading = glm::normalize(horizontal);
			const float angle = std::atan2(glm::dot(up, glm::cross(forward, heading)),
				glm::clamp(glm::dot(forward, heading), -1.0f, 1.0f));
			const float body_angle = looking ? angle - glm::clamp(angle, -1.2f, 1.2f) : angle;
			forward = glm::angleAxis(glm::clamp(body_angle, -dt * 4.0f, dt * 4.0f), up) * forward;
		}
		const float pitch = looking ? glm::clamp(std::atan2(glm::dot(requested, up), glm::length(horizontal)), -0.8f, 0.8f) : 0.0f;
		head_rotation.y += glm::clamp(pitch - head_rotation.y, -dt * 4.0f, dt * 4.0f);
		float yaw = 0.0f;
		if(looking && glm::length(horizontal) > 1e-6f)
		{
			const vec3 heading = glm::normalize(horizontal);
			yaw = glm::clamp(std::atan2(glm::dot(up, glm::cross(forward, heading)),
				glm::dot(forward, heading)), -1.2f, 1.2f);
		}
		head_rotation.x += glm::clamp(yaw - head_rotation.x, -dt * 4.0f, dt * 4.0f);
		target_look_direction = vec3(0.0f);
	}

}
