#include <utility>

#include <danikk_space_engine/component/point/camera.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		void updateCameraSurfaceFrame(CameraComponent& camera, const PointComponentSlot& slot, const World& world)
		{
			if(!world.data.isChunkActive(slot.chunk_id)) return;
			const Chunk& camera_chunk = world.data.getChunk(slot.chunk_id);
			const WorldBody& body = world.data.getWorldBody(camera_chunk.world_body_id);
			const glm::dvec3 radial = world.getCameraWorldPosition() - body.position;
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
			else if(cosine < 0.0f)
			{
				camera.yaw_reference_forward = -camera.yaw_reference_forward;
			}

			camera.surface_up = new_up;
			camera.yaw_reference_forward -= camera.surface_up
				* glm::dot(camera.yaw_reference_forward, camera.surface_up);
			const float forward_length = glm::length(camera.yaw_reference_forward);
			if(forward_length <= 0.000001f)
			{
				const vec3 reference_axis = glm::abs(camera.surface_up.z) < 0.9f
					? vec3(0.0f, 0.0f, 1.0f)
					: vec3(1.0f, 0.0f, 0.0f);
				camera.yaw_reference_forward = glm::normalize(
					reference_axis - camera.surface_up * glm::dot(reference_axis, camera.surface_up));
			}
			else
			{
				camera.yaw_reference_forward /= forward_length;
			}
		}

		vec3 getHorizontalHeading(const CameraComponent& camera)
		{
			const vec3 up = glm::normalize(camera.surface_up);
			const vec3 forward = glm::normalize(
				camera.yaw_reference_forward - up * glm::dot(camera.yaw_reference_forward, up));
			const vec3 right = glm::normalize(glm::cross(forward, up));
			return glm::normalize(forward * cos(camera.rotation.x) + right * sin(camera.rotation.x));
		}
	}

	vec3 CameraComponent::getFront() const
	{
		const vec3 up = glm::normalize(surface_up);
		const vec3 heading = getHorizontalHeading(*this);
		return glm::normalize(heading * cos(rotation.y) + up * sin(rotation.y));
	}

	vec3 CameraComponent::getUp() const
	{
		const vec3 front = getFront();
		const vec3 heading = getHorizontalHeading(*this);
		const vec3 right = glm::normalize(glm::cross(heading, surface_up));
		const vec3 base_up = glm::normalize(glm::cross(right, front));
		const mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, front);
		return glm::normalize(vec3(roll_matrix * vec4(base_up, 0.0f)));
	}

	vec3 CameraComponent::getRight() const
	{
		return glm::normalize(glm::cross(getFront(), getUp()));
	}

	void constructCameraComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("Camera component construction received null destination");
		}

		if(init_data == nullptr)
		{
			new (destination) CameraComponent();
		}
		else
		{
			new (destination) CameraComponent(*static_cast<const CameraComponent*>(init_data));
		}
	}

	void moveCameraComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Camera component move received null memory");
		}

		CameraComponent* source_component = static_cast<CameraComponent*>(source);
		new (destination) CameraComponent(std::move(*source_component));
		std::destroy_at(source_component);
	}

	void destroyCameraComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Camera component destruction received null memory");
		}
		std::destroy_at(static_cast<CameraComponent*>(data));
	}

	void tickCameraComponent(PointComponentSlot& slot, World& world)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Camera component tick received null data");
		}
		if(!world.data.isPointActive(slot.point_id))
		{
			danikk_engine::formatFatalError("Camera component references invalid point %", slot.point_id);
		}

		CameraComponent& camera = *static_cast<CameraComponent*>(slot.getData());
		updateCameraSurfaceFrame(camera, slot, world);
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_down = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		if(alt_down)
		{
			return;
		}

		const vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
		const float roll_cos = cos(camera.rotation.z);
		const float roll_sin = sin(camera.rotation.z);
		camera.rotation.x += cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
		camera.rotation.y += -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
		camera.rotation.y = glm::clamp(camera.rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

		const float frame_delay = getTargetFrameDelay();
		const float roll_delta = 1.5f * frame_delay;
		const int q_state = getKeyboardState(keyboard_buttons::q);
		if(q_state == button_states::press || q_state == button_states::hold)
		{
			camera.rotation.z -= roll_delta;
		}
		const int e_state = getKeyboardState(keyboard_buttons::e);
		if(e_state == button_states::press || e_state == button_states::hold)
		{
			camera.rotation.z += roll_delta;
		}

		float speed = frame_delay;
		if(getKeyboardState(keyboard_buttons::left_shift))
		{
			speed *= 10.0f;
		}
		if(getKeyboardState(keyboard_buttons::left_control))
		{
			speed *= 10.0f;
		}

		vec3 direction(0.0f);
		if(getKeyboardState(keyboard_buttons::w))
		{
			direction += camera.getFront();
		}
		if(getKeyboardState(keyboard_buttons::s))
		{
			direction -= camera.getFront();
		}
		if(getKeyboardState(keyboard_buttons::a))
		{
			direction -= camera.getRight();
		}
		if(getKeyboardState(keyboard_buttons::d))
		{
			direction += camera.getRight();
		}
		world.data.getPoint(slot.point_id).position += direction * speed;
		updateCameraSurfaceFrame(camera, slot, world);
	}

	void frameCameraComponent(PointComponentSlot& slot, World&)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Camera component frame received null data");
		}

		const CameraComponent& camera = *static_cast<const CameraComponent*>(slot.getData());
		if(camera.vertical_fov_degrees <= 0.0f || camera.vertical_fov_degrees >= 180.0f
			|| camera.near_clip_distance <= 0.0f
			|| camera.far_clip_distance <= camera.near_clip_distance)
		{
			danikk_engine::formatFatalError("Camera component has invalid projection parameters");
		}

		const mat4 projection = glm::perspective(
			glm::radians(camera.vertical_fov_degrees),
			screen_ratio_gz,
			camera.near_clip_distance,
			camera.far_clip_distance);
		setProjectionMatrix(projection);
	}
}
