#pragma once

#include <danikk_engine/danikk_engine.h>

#include <danikk_space_engine/object/gameobject.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	class Controller
	{
	public:
		GameObject* target;

		virtual void tick() = 0;
	};

	class LocalPlayerController : public Controller
	{
	public:
		void tick()
		{
			vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
			float roll_cos = cos(target->rotation.z);
			float roll_sin = sin(target->rotation.z);
			float yaw_delta = cursor_delta.x * roll_cos + cursor_delta.y * roll_sin;
			float pitch_delta = -cursor_delta.x * roll_sin + cursor_delta.y * roll_cos;
			target->rotation.x += yaw_delta;
			target->rotation.y += pitch_delta;

			target->rotation.y = glm::clamp(target->rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

			float roll_delta = 1.5f * getTargetFrameDelay();
			if(getKeyboardState(keyboard_buttons::q))
			{
				target->rotation.z -= roll_delta;
			}
			if(getKeyboardState(keyboard_buttons::e))
			{
				target->rotation.z += roll_delta;
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
				direction += target->getFront();
			}
			if(getKeyboardState(keyboard_buttons::s))
			{
				direction += -target->getFront();
			}
			vec3 camera_right = target->getRight();
			if(getKeyboardState(keyboard_buttons::a))
			{
				direction -= camera_right;
			}
			if(getKeyboardState(keyboard_buttons::d))
			{
				direction += camera_right;
			}
			direction *= speed;
			target->pos += direction;
		}
	};
}
