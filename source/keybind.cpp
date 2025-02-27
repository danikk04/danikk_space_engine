#include <keybind.h>

#include <danikk_engine/input.h>
#include <default.h>
#include <manager.h>
#include <component/component.h>
#include <component/asteroid.h>
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
		Object asteroid;
		/*ComponentStorage asteroid_components;
		asteroid_components.addComponent();
		component_class_storage.addComponent(AsteroidComponent::id);
		component_class_storage.getClassById(WorldObjectComponent::id);
		component_class_storage.getClassById(AsteroidComponent::id);
		asteroid.setComponents(component_class_storage);
		game_manager.map_root.childs.push();*/
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
