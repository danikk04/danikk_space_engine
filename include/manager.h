#pragma once

#include <default.h>

#include <object/object.h>
#include <object/world.h>
#include <object/player.h>

namespace danikk_space_engine
{
	class Manager
	{
	public:
		WorldObject* map_root;
		WorldObject* camera_object;

		void tick();

		void frame();

		void init();
	};

	Object* getParent();

	Object* getParentOfParent();

	extern Manager game_manager;
}
