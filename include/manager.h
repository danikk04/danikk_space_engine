#pragma once

#include <default.h>

#include <object/object.h>
#include <object/world.h>
#include <object/camera.h>
#include <controller/controller.h>

namespace game
{
	class Manager
	{
	public:
		WorldObject map_root;
		DynamicArray<Controller*> controller_array;
		Camera* main_camera = NULL;

		void tick();

		void frame();

		void init();
	};

	extern Manager game_manager;
}
