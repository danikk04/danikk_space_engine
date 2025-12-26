#pragma once

#include <default.h>

#include <object/object.h>
#include <object/world.h>
#include <block/allocator.h>
#include <object/player.h>

namespace danikk_space_engine
{
	class Manager
	{
	public:
		Object* map_root;
		Object* camera_object;

		void tick();

		void frame();

		void init();
	};

	Object* getParent();

	Object* getParentOfParent();

	extern Manager game_manager;
	extern MonolithAllocator entity_allocator;
}
