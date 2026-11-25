#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	void CameraSystem::init()
	{
		danikk_framework::formatLogInfo("Init CameraSystem");
		unbind();
	}
	void CameraSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown CameraSystem");
		unbind();
	}
	namespace
	{
		/** @brief Applies yaw to the camera's tangent-plane forward reference. */
		vec3 getHorizontalHeading(const CameraSystem& camera)
		{
			const vec3 up = glm::normalize(camera.surface_up);
			const vec3 forward = glm::normalize(
				camera.yaw_reference_forward - up * glm::dot(camera.yaw_reference_forward, up));
			const vec3 right = glm::normalize(glm::cross(forward, up));
			return glm::normalize(forward * cos(camera.rotation.x) + right * sin(camera.rotation.x));
		}
	}

	/** @copydoc CameraSystem::bind */
	void CameraSystem::bind(WorldBodyId new_body_id, const glm::dvec3& new_body_local_position)
	{
		body_id = new_body_id;
		body_local_position = new_body_local_position;
		surface_up = vec3(0.0f, 0.0f, 1.0f);
		yaw_reference_forward = vec3(1.0f, 0.0f, 0.0f);
		rotation = vec3(0.0f);
		vertical_fov_degrees = 90.0f;
		near_clip_distance = 0.1f;
		far_clip_distance = 10000.0f;
		setProjectionMode(CameraProjectionMode::near_clip,
			near_clip_distance, far_clip_distance);
	}

	/** @copydoc CameraSystem::unbind */
	void CameraSystem::unbind()
	{
		body_id = static_cast<WorldBodyId>(-1);
		body_local_position = glm::dvec3(0.0);
		setProjectionMode(CameraProjectionMode::near_clip,
			near_clip_distance, far_clip_distance);
	}

	/** @copydoc CameraSystem::setProjectionMode */
	void CameraSystem::setProjectionMode(CameraProjectionMode mode,
		float new_near_clip_distance, float new_far_clip_distance)
	{
		projection_mode = mode;
		projection_near_clip_distance = new_near_clip_distance;
		projection_far_clip_distance = new_far_clip_distance;
	}

	/** @copydoc CameraSystem::isActive */
	bool CameraSystem::isActive() const
	{
		return body_id != static_cast<WorldBodyId>(-1)
			&& world.getWorldBodySystem().isWorldBodyActive(body_id);
	}

	/** @copydoc CameraSystem::getFront */
	vec3 CameraSystem::getFront() const
	{
		const vec3 up = glm::normalize(surface_up);
		const vec3 heading = getHorizontalHeading(*this);
		return glm::normalize(heading * cos(rotation.y) + up * sin(rotation.y));
	}

	/** @copydoc CameraSystem::getUp */
	vec3 CameraSystem::getUp() const
	{
		const vec3 front = getFront();
		const vec3 heading = getHorizontalHeading(*this);
		const vec3 right = glm::normalize(glm::cross(heading, surface_up));
		const vec3 base_up = glm::normalize(glm::cross(right, front));
		const mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, front);
		return glm::normalize(vec3(roll_matrix * vec4(base_up, 0.0f)));
	}

	/** @copydoc CameraSystem::getRight */
	vec3 CameraSystem::getRight() const
	{
		return glm::normalize(glm::cross(getFront(), getUp()));
	}

	/** @copydoc CameraSystem::getWorldPosition */
	glm::dvec3 CameraSystem::getWorldPosition() const
	{
		const WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
		return body.position + body.rotation * body_local_position;
	}

	/** @copydoc CameraSystem::moveBodyLocal */
	void CameraSystem::moveBodyLocal(const glm::dvec3& displacement)
	{
		if(!isActive()) return;
		body_local_position += displacement;
	}

	/** @copydoc CameraSystem::applyProjection */
	void CameraSystem::applyProjection() const
	{
		DSE_WATCH();
		if(!isActive()) return;
		if(vertical_fov_degrees <= 0.0f || vertical_fov_degrees >= 180.0f
			|| projection_near_clip_distance <= 0.0f
			|| projection_far_clip_distance <= projection_near_clip_distance
			|| (projection_mode != CameraProjectionMode::near_clip
				&& projection_mode != CameraProjectionMode::far_clip))
			danikk_engine::formatFatalError("Camera has invalid projection parameters");
		setProjectionMatrix(glm::perspective(glm::radians(vertical_fov_degrees), screen_ratio_gz,
			projection_near_clip_distance, projection_far_clip_distance));
	}
}
