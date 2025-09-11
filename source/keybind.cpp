#include <keybind.h>

#include <danikk_engine/input.h>
#include <default.h>
#include <manager.h>

namespace danikk_space_engine
{
	KeyBindCollection key_binds;

	void closeGame()
	{
		danikk_engine::exit();
	}

	void KeyBindCollection::init()
	{
		base_keybinds.bind(keyboard_buttons::escape, closeGame);
	}

	void KeyBindCollection::frame()
	{
		base_keybinds.checkBindingPress();
	}

	void KeyBindCollection::bind(const KeyBindKeys& keys, key_bind_event_t event)
	{
		base_keybinds.bind(keys, event);
	}

	void KeyBindCollection::bind(const KeyBindKeys& keys, key_bind_args_event_t event, void* arg_ptr)
	{
		base_keybinds.bind(keys, event, arg_ptr);
	}
}
