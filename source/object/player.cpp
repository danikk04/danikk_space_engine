#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <object/player.h>

#include <default.h>
#include <manager.h>
#include <object/world.h>

namespace danikk_space_engine
{
	void Player::tick()
	{
		vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
		rotation.x += cursor_delta.x;
		rotation.y += cursor_delta.y;

		rotation.y = glm::clamp(rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

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
		if(getKeyboardState(keyboard_buttons::a))
		{
			direction += glm::rotateY(getFlatFront(), 0.5f * pi);
		}
		else if(getKeyboardState(keyboard_buttons::d))
		{
			direction += glm::rotateY(getFlatFront(), 1.5f * pi);
		}
		direction *= speed;
		pos += direction;

		if(getKeyboardState(keyboard_buttons::left_alt) == button_states::hold)
		{
			Object* obj = NULL;
			if(obj != NULL)
			{
				getParentObject()->addChild(obj);
			}
		}
	}
}
