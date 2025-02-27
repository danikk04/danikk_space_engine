#pragma once

#include <default.h>

#include <object.h>

namespace sotiria_project
{
	class Manager
	{
	public:
		Object map_root;

		void tick();

		void frame();

		void init();
	};

	extern Manager game_manager;
}
