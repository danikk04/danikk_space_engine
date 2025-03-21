#include <manager.h>
#include <object/camera.h>
#include <object/meshed.h>
#include <object_preset/container.h>
#include <danikk_engine/built_in_meshes.h>
#include <danikk_framework/danikk_framework.h>
#include <controller/player.h>
#include <asset.h>

namespace game
{
	Manager game_manager;
	thread_local Array<Object*, 16> object_stack;

	void Manager::tick()
	{
		object_stack.push(&map_root);
		for(Object* o : map_root.childs)
		{
			o->tick();
		}
		for(Controller* c : controller_array)
		{
			c->control();
		}
		object_stack.pop();
	}

	void Manager::frame()
	{
		static_asset_collection.base3d_shader.use();
		object_stack.push(&map_root);
		for(Object* o : map_root.childs)
		{
			o->frame();
		}
		object_stack.pop();
	}

	void Manager::init()
	{
		map_root.world_matrix = mat4(1.0f);

		Camera* cam = new Camera();
		cam->pos = vec3(3.0f, 3.0f, 3.0f);
		map_root.childs.push(cam);

		Container* obj = new Container();
		map_root.childs.push(obj);

		Controller* player_controller = new PlayerController();
		player_controller->controllable_object = cam;
		controller_array.push(player_controller);
	}
}
