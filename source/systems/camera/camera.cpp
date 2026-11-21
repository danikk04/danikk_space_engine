#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>

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
		/** @brief Parallel-transports the camera heading as its radial up vector changes. */
		void updateCameraSurfaceFrame(CameraSystem& camera, const World& world)
		{
			const Point& point = world.point_data.getPoint(camera.getPointId());
			if(!world.world_body_data.isWorldBodyActive(point.world_body_id)) return;
			const WorldBody& body = world.world_body_data.getWorldBody(point.world_body_id);
			const glm::dvec3 radial = camera.getWorldPosition() - body.position;
			const double radial_length = glm::length(radial);
			if(radial_length <= 0.000001) return;

			const vec3 new_up = vec3(radial / radial_length);
			const vec3 old_up = glm::normalize(camera.surface_up);
			const vec3 transport_axis = glm::cross(old_up, new_up);
			const float sine = glm::length(transport_axis);
			const float cosine = glm::clamp(glm::dot(old_up, new_up), -1.0f, 1.0f);
			if(sine > 0.000001f)
			{
				const float angle = atan2(sine, cosine);
				const mat4 transport = glm::rotate(mat4(1.0f), angle, transport_axis / sine);
				camera.yaw_reference_forward = vec3(transport * vec4(camera.yaw_reference_forward, 0.0f));
			}
			else if(cosine < 0.0f) camera.yaw_reference_forward = -camera.yaw_reference_forward;

			camera.surface_up = new_up;
			camera.yaw_reference_forward -= camera.surface_up
				* glm::dot(camera.yaw_reference_forward, camera.surface_up);
			const float forward_length = glm::length(camera.yaw_reference_forward);
			if(forward_length <= 0.000001f)
			{
				const vec3 reference_axis = glm::abs(camera.surface_up.z) < 0.9f
					? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
				camera.yaw_reference_forward = glm::normalize(
					reference_axis - camera.surface_up * glm::dot(reference_axis, camera.surface_up));
			}
			else camera.yaw_reference_forward /= forward_length;
		}

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
	void CameraSystem::bind(PointId new_point_id)
	{
		point_id = new_point_id;
		surface_up = vec3(0.0f, 0.0f, 1.0f);
		yaw_reference_forward = vec3(1.0f, 0.0f, 0.0f);
		rotation = vec3(0.0f);
		vertical_fov_degrees = 90.0f;
		near_clip_distance = 0.1f;
		far_clip_distance = 10000.0f;
	}

	/** @copydoc CameraSystem::unbind */
	void CameraSystem::unbind()
	{
		point_id = static_cast<PointId>(-1);
	}

	/** @copydoc CameraSystem::isActive */
	bool CameraSystem::isActive() const
	{
		return point_id != static_cast<PointId>(-1) && world.point_data.isPointActive(point_id)
			&& world.world_body_data.isWorldBodyActive(world.point_data.getPoint(point_id).world_body_id);
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
		return world.getPointWorldPosition(point_id);
	}

	/** @copydoc CameraSystem::tick */
	void CameraSystem::tick()
	{
		DSE_WATCH();
		if(!isActive()) return;
		const bool has_controller = world.controller.controls();
		if(has_controller && world.controller.tool_catalog_open) return;
		updateCameraSurfaceFrame(*this, world);

		const vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
		const int left_mouse_state = getMouseState(mouse_buttons::left);
		const bool left_mouse_down = left_mouse_state == button_states::press
			|| left_mouse_state == button_states::hold;
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_pressed = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		const bool drag_rotation_mode = has_controller
			&& world.controller.active_tool == ToolId::drag
			&& world.controller.selected_tool_action == ToolActionId::drag
			&& left_mouse_down && alt_pressed;
		if(!drag_rotation_mode)
		{
			const float roll_cos = cos(rotation.z);
			const float roll_sin = sin(rotation.z);
			rotation.x += cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
			rotation.y += -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
			rotation.y = glm::clamp(rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);
		}

		const float frame_delay = getTargetFrameDelay();
		const float roll_delta = 1.5f * frame_delay;
		const int q_state = getKeyboardState(keyboard_buttons::q);
		if(q_state == button_states::press || q_state == button_states::hold) rotation.z -= roll_delta;
		const int e_state = getKeyboardState(keyboard_buttons::e);
		if(e_state == button_states::press || e_state == button_states::hold) rotation.z += roll_delta;

		const float speed = (has_controller ? world.controller.camera_movement_speed : 1.0f) * frame_delay;
		vec3 direction(0.0f);
		if(getKeyboardState(keyboard_buttons::w)) direction += getFront();
		if(getKeyboardState(keyboard_buttons::s)) direction -= getFront();
		if(getKeyboardState(keyboard_buttons::a)) direction -= getRight();
		if(getKeyboardState(keyboard_buttons::d)) direction += getRight();
		Point& point = world.point_data.getPoint(point_id);
		world.setPointBodyLocalPosition(point_id,
			point.body_local_position + glm::dvec3(direction * speed));
		updateCameraSurfaceFrame(*this, world);
	}

	/** @copydoc CameraSystem::frame */
	void CameraSystem::frame() const
	{
		DSE_WATCH();
		if(!isActive()) return;
		if(vertical_fov_degrees <= 0.0f || vertical_fov_degrees >= 180.0f
			|| near_clip_distance <= 0.0f || far_clip_distance <= near_clip_distance)
			danikk_engine::formatFatalError("Camera has invalid projection parameters");
		setProjectionMatrix(glm::perspective(glm::radians(vertical_fov_degrees), screen_ratio_gz,
			near_clip_distance, far_clip_distance));
	}
}
