#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <manager.h>
#include <asset.h>
#include <material.h>

#include <object/camera.h>
#include <preset/container.h>
#include <preset/block/fill.h>

#include <object/block_map.h>
#include <object/camera.h>
#include <block/context.h>

#include <controller/player.h>

#include <block/container.h>
#include <block/data.h>
#include <block/allocator.h>

namespace danikk_space_engine
{
	Manager game_manager;
	MonolithAllocator entity_allocator;

	void Manager::tick()
	{
		object_stack.push(&map_root);
		for(Object* o : map_root.childs)
		{
			o->tick();
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

		BlockMapObject* block_map = new BlockMapObject();
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = block_map;
		map_root.childs.push(block_map);

		for(uvec3 pos : TensorIterable<uvec3(2,2,2)>())
		{
			ivec3 region_pos = (ivec3)pos;
			current_block_context->region = &(*block_map)[region_pos];
			BlockSlot block;
			BlockBaseHeader& header = block.createHeader();
			header.id = SolidRaw::id;
			header.main_material_id = getMaterialID("granite");
			header.main_material_mass = 1.0f;
			//fillRegionCorners(block);
			//fillRegionCenters(block);
			//fillRegionLine(block, pos_type(0,0,0), pos_type(31,31,31));
			//fillRegionLine(block, pos_type(31,0,0), pos_type(0,31,31));
			fillRegion(block);

			current_block_context->region->checkExits();
			current_block_context->region->regenerateMesh();
		}

		main_camera = new Camera();
		main_camera->pos = vec3(16.0f, 16.0f, 16.0f);
		block_map->childs.push(main_camera);

		Controller* player_controller = new PlayerController();
		main_camera->childs.push(player_controller);
	}

	Object* getParent()
	{
		return object_stack[object_stack.size() - 1];
	}

	Object* getParentOfParent()
	{
		return object_stack[object_stack.size() - 2];
	}
}
