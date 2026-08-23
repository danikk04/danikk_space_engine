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
		controller.tick();
		map_root->tick();
	}

	void Manager::frame()
	{
		static_asset_collection.base3d_shader.use();
		mat4 view = glm::lookAt(camera->pos, camera->pos + camera->getFront(), vec3(0, 1, 0));
		mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		setViewMatrix(view);
		setProjectionMatrix(projection);
		map_root->frame();
	}

	void Manager::init()
	{
		map_root = new GameObject();
		camera = new GameObject();
		controller.target = camera;
	}

	thread_local Array<GameObject*, 16> object_stack;

	GameObject* getParent()
	{
		return object_stack[object_stack.size() - 1];
	}

	void pushParent(GameObject* object)
	{
		object_stack.push(object);
	}

	void popParent()
	{
		object_stack.pop();
	}
}
