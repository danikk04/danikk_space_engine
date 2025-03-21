#include <keybind.h>

#include <danikk_engine/input.h>
#include <default.h>
#include <manager.h>

namespace game
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
}
