#pragma once

#include <danikk_space_engine/default.h>

#include <danikk_space_engine/object/gameobject.h>
#include <danikk_space_engine/controller.h>

namespace danikk_space_engine
{
	class World
	{
	public:
		~World();

		GameObject* map_root = nullptr;
		GameObject* camera = nullptr;
		LocalPlayerController controller;

		void tick();

		void frame();

		void init();
	};

	extern World world;
}
