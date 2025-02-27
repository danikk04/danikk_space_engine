#include <keybind.h>

#include <danikk_engine/input.h>
#include <default.h>
#include <manager.h>
#include <component/world_object.h>

namespace sotiria_project
{
	KeyBindCollection key_binds;

	void closeGame()
	{
		danikk_engine::exit();
	}

	void addAsteroid()
	{

	}

	void KeyBindCollection::init()
	{
		base_keybinds.bind(keyboard_buttons::escape, closeGame);
		base_keybinds.bind(keyboard_buttons::t, addAsteroid);
	}

	void KeyBindCollection::frame()
	{
		base_keybinds.checkBindingPress();
	}
}
