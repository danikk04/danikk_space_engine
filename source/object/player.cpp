#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <object/world.h>
#include <object/projectile.h>
#include <object/player.h>

#include <default.h>
#include <manager.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		void PlayerController::tick()
		{
			object_tags::World* controllable_as_world = object_stack.peek()->getTag<object_tags::World>();
			assert(controllable_as_world != NULL);
			vec2 cursor_delta = getCursorDelta() * getMouseSensivity();
			controllable_as_world->rotation.x += cursor_delta.x;
			controllable_as_world->rotation.y += cursor_delta.y;

			controllable_as_world->rotation.y = glm::clamp(controllable_as_world->rotation.y, -pi_half + 0.0001f, pi_half - 0.0001f);

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
				direction += controllable_as_world->getFront();
			}
			if(getKeyboardState(keyboard_buttons::s))
			{
				direction += -controllable_as_world->getFront();
			}
			if(getKeyboardState(keyboard_buttons::a))
			{
				direction += glm::rotateY(controllable_as_world->getFlatFront(), 0.5f * pi);
			}
			else if(getKeyboardState(keyboard_buttons::d))
			{
				direction += glm::rotateY(controllable_as_world->getFlatFront(), 1.5f * pi);
			}
			direction *= speed;
			controllable_as_world->pos += direction;

			if(getKeyboardState(keyboard_buttons::left_alt) == button_states::hold)
			{
				Object* obj = NULL;
				if(getKeyboardState(keyboard_buttons::f) == button_states::hold)
				{
					//obj = new PlasmaBullet();
				}
				else if(getKeyboardState(keyboard_buttons::g) == button_states::hold)
				{
					obj = createDestroyerProjectile(controllable_as_world->pos, controllable_as_world->rotation);
				}
				if(obj != NULL)
				{
					getParentObject()->addChild(obj);
				}
			}
		}
	}
}
