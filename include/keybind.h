#pragma once

#include <default.h>

#include <danikk_engine/key_bind.h>

namespace sotiria_project
{
	class KeyBindCollection
	{
		KeyBindArray base_keybinds;
	public:
		void init();

		void frame();
	};

	extern KeyBindCollection key_binds;
}

