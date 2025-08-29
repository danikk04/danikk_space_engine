#include <manager.h>
#include <object/camera.h>
#include <preset/container.h>
#include <object/block_map.h>
#include <preset/block/base.h>
#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <controller/player.h>
#include <block/container.h>
#include <block/data.h>
#include <asset.h>

namespace danikk_space_engine
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
		main_camera->pos = vec3(0.0f, 0.0f, 0.0f);
		map_root.childs.push(main_camera);

		Controller* player_controller = new PlayerController();
		player_controller->controllable_object = main_camera;
		controller_array.push(player_controller);

		testScenario();
	}


	void Manager::testScenario()
	{
		BlockMapObject* block_map = new BlockMapObject();
		map_root.childs.push(block_map);

		for(uvec3 pos : TensorIterable<uvec3(2,2,2)>())
		{
			ivec3 global_pos = (ivec3)pos - ivec3(1,1,1);
			BlockMapRegion& region = (*block_map)[global_pos];
			region.setAsCurrent();
			BlockSlot block;
			BlockBaseHeader& header = block.createHeader();
			header.id = SolidRaw::id;
			//block.data.resize
			fillRegionCorners(region, block);
		}
	}
}
