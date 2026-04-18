#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <manager.h>
#include <asset.h>
#include <material.h>

namespace danikk_space_engine
{
	Manager game_manager;

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
