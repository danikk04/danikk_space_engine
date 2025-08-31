#pragma once

#include <default.h>

#include <object/object.h>
#include <object/world.h>
#include <object/camera.h>
#include <block/allocator.h>
#include <controller/controller.h>

namespace danikk_space_engine
{
	class Manager
	{
	public:
		WorldObject map_root;
		Camera* main_camera = NULL;

		void tick();

		void frame();

		void init();
	};

	Object* getParent();

	Object* getParentOfParent();

	extern Manager game_manager;
	extern RegionAllocator entity_allocator;
}
