#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/manager.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/object/constructed.h>
#include <danikk_space_engine/object/ground.h>

namespace danikk_space_engine
{
	Manager game_manager;

	Manager::~Manager()
	{
		if(map_root != nullptr)
		{
			map_root->destroy();
			map_root = nullptr;
			camera = nullptr;
		}
	}

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
		vec3 camera_front = camera->getFront();
		mat4 view = glm::lookAt(camera->pos, camera->pos + camera_front, camera->getUp());
		mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		setViewMatrix(view);
		setProjectionMatrix(projection);
		map_root->frame();
	}

	void Manager::init()
	{
		map_root = GameObject::create();
		camera = GameObject::create();
		map_root->local_matrix = mat4(1.0f);
		map_root->world_matrix = mat4(1.0f);
		map_root->addChild(camera);

		GroundObject* ground = GroundObject::create();
		map_root->addChild(ground);

		controller.target = camera;

	}

}
