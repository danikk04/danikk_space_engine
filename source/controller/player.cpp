#include <danikk_engine/input.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/glm.h>

#include <object/world.h>
#include <preset/container.h>
#include <object/camera.h>
#include <controller/player.h>

#include <default.h>
#include <manager.h>

namespace danikk_space_engine
{
	 void PlayerController::control()
	 {
		WorldObject* controllable_as_world = dynamic_cast<WorldObject*>(Controller::controllable_object);
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

		WorldObject* controllable_as_camera = dynamic_cast<Camera*>(Controller::controllable_object);
		if(controllable_as_camera != NULL)
		{
			if(getKeyboardState(keyboard_buttons::f) == button_states::press)
			{
				Container* obj = new Container();
				obj->pos = controllable_as_world->pos;
				obj->rotation = controllable_as_world->rotation;
				obj->speed = controllable_as_camera->getFront() * getTargetFrameDelay();

				game_manager.map_root.childs.push(obj);
			}
		}
	 }
}
