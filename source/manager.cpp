#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/manager.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{
	Manager game_manager;

	void Manager::tick()
	{
		controller.tick();
		for(GameObject* child : map_root->getChilds())
		{
			child->tick();
		}
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
		map_root->local_matrix = mat4(1.0f);
		map_root->world_matrix = mat4(1.0f);
		map_root->addChild(camera);

		//Gameobject* ground = new GameObject();
		//map_root.childs.push(ground);

		controller.target = camera;
	}

}
