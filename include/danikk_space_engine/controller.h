#pragma once

#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/point.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>
#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	class Controller
	{
	public:
		PointId target_point_id = 0;
		virtual void tick(DynamicArray<Point>& points) = 0;
	};

	class LocalPlayerController : public Controller
	{
	public:
		vec3 rotation = vec3(0.0f);

		vec3 getFront() const
		{
			return vec3(
					(float)cos(rotation.y) * (float)cos(rotation.x),
					(float)sin(rotation.y),
					(float)cos(rotation.y) * (float)sin(rotation.x));
		}

		vec3 getUp() const
		{
			mat4 roll_matrix = glm::rotate(mat4(1.0f), rotation.z, getFront());
			return vec3(roll_matrix * vec4(0.0f, 1.0f, 0.0f, 0.0f));
		}

		vec3 getRight() const
		{
			return glm::normalize(glm::cross(getFront(), getUp()));
		}

		void tick(DynamicArray<Point>& points) override
		{
			Point& target = points[target_point_id];
			vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
			float roll_cos = cos(rotation.z);
			float roll_sin = sin(rotation.z);
			float yaw_delta = cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
			float pitch_delta = -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
			rotation.x += yaw_delta;
			rotation.y += pitch_delta;

			rotation.y = glm::clamp(rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

			float roll_delta = 1.5f * getTargetFrameDelay();
			if(getKeyboardState(keyboard_buttons::q))
			{
				rotation.z -= roll_delta;
			}
			if(getKeyboardState(keyboard_buttons::e))
			{
				rotation.z += roll_delta;
			}

			float speed = getTargetFrameDelay();
			if(getKeyboardState(keyboard_buttons::left_shift))
			{
				speed *= 10;
			}
			if(getKeyboardState(keyboard_buttons::left_control))
			{
				speed *= 10;
			}

			vec3 direction = vec3(0.0f);
			if(getKeyboardState(keyboard_buttons::w))
			{
				direction += getFront();
			}
			if(getKeyboardState(keyboard_buttons::s))
			{
				direction += -getFront();
			}
			vec3 camera_right = getRight();
			if(getKeyboardState(keyboard_buttons::a))
			{
				direction -= camera_right;
			}
			if(getKeyboardState(keyboard_buttons::d))
			{
				direction += camera_right;
			}
			direction *= speed;
			target.position += direction;
		}
	};
}
