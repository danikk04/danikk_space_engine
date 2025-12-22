#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <object/world.h>
/*#include <object/entity/particle/plasma.h>
#include <object/entity/particle/destroyer.h>*/
#include <controller/player.h>

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

			/*Camera* controllable_as_camera = dynamic_cast<Camera*>(object_stack.peek());
			if(controllable_as_camera != NULL)
			{
				if(getKeyboardState(keyboard_buttons::left_alt) == button_states::hold)
				{
					Particle* obj = NULL;
					if(getKeyboardState(keyboard_buttons::f) == button_states::hold)
					{
						obj = new PlasmaBullet();
					}
					else if(getKeyboardState(keyboard_buttons::g) == button_states::hold)
					{
						obj = new DestroyerBullet();
					}
					if(obj != NULL)
					{
						vec3 randomized_rotation = controllable_as_world->rotation + vec3(randVec2(), 0.0f) * 0.3f;
						obj->speed = getFront(randomized_rotation) * 5.0f;
						obj->pos = controllable_as_world->pos + obj->speed / vec3(4.0f);
						obj->rotation = randomized_rotation;

						getParentOfParent()->childs.push(obj);
					}
				}

				if(getKeyboardState(keyboard_buttons::f9) == button_states::hold)
				{
					for(size_t i = 0; i < 120; i ++)
					{
						PlasmaBullet* obj = new PlasmaBullet();
						vec3 randomized_rotation = vec3(randVec2(), 0.0f) * pi;
						obj->speed = getFront(randomized_rotation) * 5.0f;
						obj->pos = controllable_as_world->pos + obj->speed / vec3(4.0f);
						obj->rotation = randomized_rotation;

						getParentOfParent()->childs.push(obj);
					}
				}
			}*/
		}
	}
}
