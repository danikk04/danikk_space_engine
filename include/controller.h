#pragma once

#include <danikk_engine/danikk_engine.h>

#include <object/world.h>

#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <default.h>
#include <manager.h>
#include <object/world.h>

namespace danikk_space_engine
{
	class Controller
	{
	public:
		PosedObject* target;

		virtual void tick() = 0;
	};

	class LocalPlayerController : public Controller
	{
	public:
		void tick()
		{
			vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
			target->rotation.x += cursor_delta.x;
			target->rotation.y += cursor_delta.y;

			target->rotation.y = glm::clamp(target->rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

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
			if(getKeyboardState(keyboard_buttons::a))
			{
				direction += glm::rotateY(target->getFlatFront(), 0.5f * pi);
			}
			else if(getKeyboardState(keyboard_buttons::d))
			{
				direction += glm::rotateY(target->getFlatFront(), 1.5f * pi);
			}
			direction *= speed;
			target->pos += direction;

			if(getKeyboardState(keyboard_buttons::left_alt) == button_states::hold)
			{
				BaseObject* obj = NULL;
				if(obj != NULL)
				{
					getParentObject()->addChild(obj);
				}
			}
		}
	};
}
