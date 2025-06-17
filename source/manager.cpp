#include <manager.h>
#include <object/camera.h>
#include <preset/container.h>
#include <object/block_map.h>
#include <preset/block/base.h>
#include <danikk_framework/danikk_framework.h>
#include <controller/player.h>
#include <block/container.h>
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
		main_camera->setViewMatrix();
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

		main_camera = new Camera();
		main_camera->pos = vec3(3.0f, 3.0f, 3.0f);
		map_root.childs.push(main_camera);

		BlockMapObject* block_map = new BlockMapObject();
		map_root.childs.push(block_map);
		fillRegion(*block_map, uvec3(), block::WoodenContainer::id);
		logInfo((size_t)block_map->filledBlockCount());

		Controller* player_controller = new PlayerController();
		player_controller->controllable_object = main_camera;
		controller_array.push(player_controller);
	}
}
