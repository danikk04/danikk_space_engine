#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <manager.h>
#include <asset.h>
#include <material.h>

#include <block/fill.h>

#include <object/block_map.h>
#include <object/tag_table.h>
#include <object/player.h>

#include <block/context.h>
#include <block/gen.h>

#include <block/data.h>
#include <block/allocator.h>

namespace danikk_space_engine
{
	Manager game_manager;
	MonolithAllocator entity_allocator;

	void Manager::tick()
	{
		object_stack.push(map_root);
		for(Object* o : map_root->iterateChilds())
		{
			o->tick();
		}
		object_stack.pop();
	}

	void Manager::frame()
	{
		static_asset_collection.base3d_shader.use();
		object_tags::World* camera_world = camera_object->getTag<object_tags::World>();
		mat4 view = glm::lookAt(camera_world->pos, camera_world->pos + camera_world->getFront(), vec3(0, 1, 0));
		mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		danikk_engine::setViewMatrix(view);
		setProjectionMatrix(projection);
		object_stack.push(map_root);
		for(Object* o : map_root->iterateChilds())
		{
			o->frame();
		}
		object_stack.pop();
	}

	void Manager::init()
	{
		map_root = Object::create<object_tags::World>();
		map_root->getTag<object_tags::World>()->world_matrix = mat4(1.0f);

		Object* block_map_object = Object::create<object_tags::World, object_tags::BlockMap>();
		object_tags::BlockMap* block_map = block_map_object->getTag<object_tags::BlockMap>();
		BlockContext context;
		current_block_context = &context;
		current_block_context->map = block_map_object->getTag<object_tags::BlockMap>();
		map_root->addChild(block_map_object);
		object_stack.push(block_map_object);

		for(uvec3 pos : TensorIterable<map_size>())
		{
			ivec3 region_pos = (ivec3)pos;
			current_block_context->region = &(*block_map)[region_pos];



			current_block_context->region->checkExits();
			current_block_context->region->regenerateMesh();
		}

		object_stack.pop();
		camera_object = Object::create<object_tags::World, object_tags::PlayerController>();

		camera_object->getTag<object_tags::World>()->pos = vec3(16.0f, 16.0f, 16.0f);
		block_map_object->addChild(camera_object);
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
