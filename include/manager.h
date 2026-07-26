#pragma once

#include <default.h>

#include <object/object.h>
#include <controller.h>
#include "object/posed.h"

namespace danikk_space_engine
{
	class Manager
	{
	public:
		PosedObject* map_root;
		PosedObject* camera;
		LocalPlayerController controller;

		void tick();

		void frame();

		void init();
	};

	BaseObject* getParent();

	BaseObject* getParentOfParent();

	extern Manager game_manager;
}
