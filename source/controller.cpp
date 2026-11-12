#include <danikk_space_engine/controller.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/polygon_highlight.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	vec3 LocalPlayerController::getFront() const
	{
		return vec3(
			static_cast<float>(cos(rotation.y)) * static_cast<float>(cos(rotation.x)),
			static_cast<float>(sin(rotation.y)),
			static_cast<float>(cos(rotation.y)) * static_cast<float>(sin(rotation.x)));
	}

	vec3 LocalPlayerController::getUp() const
	{
		const mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, getFront());
		return vec3(roll_matrix * vec4(0.0f, 1.0f, 0.0f, 0.0f));
	}

	vec3 LocalPlayerController::getRight() const
	{
		return glm::normalize(glm::cross(getFront(), getUp()));
	}

	// Updates camera rotation from mouse input and moves its target point using keyboard input.
	// Rotates mouse deltas by camera roll, clamps pitch, applies Q/E roll, then combines
	// the front and right vectors for WASD movement with shift/control speed modifiers.
	void LocalPlayerController::tick(WorldData& data)
	{
		Point& target = data.getPoint(target_point_id);
		const vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
		const float roll_cos = cos(rotation.z);
		const float roll_sin = sin(rotation.z);
		const float yaw_delta = cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
		const float pitch_delta = -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
		rotation.x += yaw_delta;
		rotation.y += pitch_delta;

		rotation.y = glm::clamp(rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

		const float roll_delta = 1.5f * getTargetFrameDelay();
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_down = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		const int q_state = getKeyboardState(keyboard_buttons::q);
		if(q_state == button_states::press || q_state == button_states::hold)
		{
			rotation.z -= roll_delta;
		}
		const int e_state = getKeyboardState(keyboard_buttons::e);
		if(!alt_down && (e_state == button_states::press || e_state == button_states::hold))
		{
			rotation.z += roll_delta;
		}

		float speed = getTargetFrameDelay();
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
			direction += getFront();
		}
		if(getKeyboardState(keyboard_buttons::s))
		{
			direction -= getFront();
		}
		const vec3 camera_right = getRight();
		if(getKeyboardState(keyboard_buttons::a))
		{
			direction -= camera_right;
		}
		if(getKeyboardState(keyboard_buttons::d))
		{
			direction += camera_right;
		}
		target.position += direction * speed;
	}

	static bool isSelected(
		const DynamicArray<FacePtr>& selected_polygons,
		const FacePtr& face)
	{
		for(const FacePtr& selected : selected_polygons)
		{
			if(selected == face)
			{
				return true;
			}
		}
		return false;
	}

	// Extrudes selected faces on Alt+E, handles left-click selection, and draws face overlays.
	// Replaces selection with the generated top faces, raycasts from the camera, toggles
	// the hit FacePtr (or clears selection on a miss), then draws selected and hovered faces.
	void LocalPlayerController::frame(World& world)
	{
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_down = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		if(alt_down
			&& getKeyboardState(keyboard_buttons::e) == button_states::press
			&& selected_polygons.size() > 0)
		{
			DynamicArray<FacePtr> extruded_faces = world.extrudeFaces(selected_polygons, 1.0f);
			if(extruded_faces.size() > 0)
			{
				selected_polygons = static_cast<DynamicArray<FacePtr>&&>(extruded_faces);
			}
		}

		const RaycastHit hit = world.raycastFromCamera();
		if(getMouseState(mouse_buttons::left) == button_states::press)
		{
			if(hit.hit)
			{
				size_t selected_index = selected_polygons.size();
				for(size_t i = 0; i < selected_polygons.size(); i++)
				{
					const FacePtr& selected = selected_polygons[i];
					if(selected == hit.face)
					{
						selected_index = i;
						break;
					}
				}

				if(selected_index < selected_polygons.size())
				{
					for(size_t i = selected_index; i + 1 < selected_polygons.size(); i++)
					{
						selected_polygons[i] = selected_polygons[i + 1];
					}
					selected_polygons.resize(selected_polygons.size() - 1);
					if(selected_polygons.size() == 0)
					{
						selected_polygons = DynamicArray<FacePtr>();
					}
				}
				else
				{
					selected_polygons.push(hit.face);
				}
			}
			else
			{
				selected_polygons = DynamicArray<FacePtr>();
			}
		}

		static Mesh highlight_mesh;
		const vec4 selected_color(0.5f, 0.5f, 0.5f, 1.0f);
		const vec4 hover_color(1.0f, 0.15f, 0.15f, 1.0f);

		for(const FacePtr& selected : selected_polygons)
		{
			drawPolygonHighlight(
				world,
				selected,
				highlight_mesh,
				selected_color);
		}

		if(hit.hit && !isSelected(selected_polygons, hit.face))
		{
			drawPolygonHighlight(
				world,
				hit.face,
				highlight_mesh,
				hover_color);
		}
	}
}
