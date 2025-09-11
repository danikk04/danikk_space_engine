#pragma once

#include <default.h>
#include <danikk_engine/key_bind.h>

namespace danikk_space_engine
{
	using danikk_engine::KeyBindKeys;
	using danikk_engine::key_bind_event_t;
	using danikk_engine::key_bind_event_t;

	class KeyBindCollection
	{
		KeyBindArray base_keybinds;
	public:
		void init();

		void frame();

		void bind(const KeyBindKeys& keys, key_bind_event_t event);

		void bind(const KeyBindKeys& keys, key_bind_args_event_t event, void* arg_ptr);
	};

	extern KeyBindCollection key_binds;
}

