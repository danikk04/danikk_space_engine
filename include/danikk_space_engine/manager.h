#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/controller.h>

namespace danikk_space_engine
{
	class Manager
	{
	public:
		GameObject* map_root;
		GameObject* camera;
		LocalPlayerController controller;

		void tick();

		void frame();

		void init();
	};

	extern Manager game_manager;
}
