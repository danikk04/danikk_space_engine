#include <utility>

#include <danikk_space_engine/component/camera.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	vec3 CameraComponent::getFront() const
	{
		return vec3(
			static_cast<float>(cos(rotation.y)) * static_cast<float>(cos(rotation.x)),
			static_cast<float>(sin(rotation.y)),
			static_cast<float>(cos(rotation.y)) * static_cast<float>(sin(rotation.x)));
	}

	vec3 CameraComponent::getUp() const
	{
		const mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, getFront());
		return vec3(roll_matrix * vec4(0.0f, 1.0f, 0.0f, 0.0f));
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
		if(slot.point_id >= world.data.getPointCount())
		{
			danikk_engine::formatFatalError("Camera component references invalid point %", slot.point_id);
		}

		CameraComponent& camera = *static_cast<CameraComponent*>(slot.getData());
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
