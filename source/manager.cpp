#include <danikk_framework/danikk_framework.h>
#include <danikk_framework/tensor.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/manager.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/object/constructed.h>

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
		mat4 view = glm::lookAt(camera->pos, camera->pos + camera->getFront(), vec3(0, 1, 0));
		mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		setViewMatrix(view);
		setProjectionMatrix(projection);
		map_root->frame();
	}

	void Manager::init()
	{
		map_root = GameObject::create();
		camera = GameObject::create();
		if(map_root == nullptr || camera == nullptr)
		{
			if(map_root != nullptr) map_root->destroy();
			if(camera != nullptr) camera->destroy();
			map_root = nullptr;
			camera = nullptr;
			return;
		}
		map_root->local_matrix = mat4(1.0f);
		map_root->world_matrix = mat4(1.0f);
		map_root->addChild(camera);

		controller.target = camera;

		struct TestObject
		{
			vec3 size;
			vec3 position;
			vec3 rotation;
			vec4 color;
		};
		const TestObject test_objects[] =
		{
			{vec3(1, 1, 1), vec3(5, 0, -3), vec3(0.0f, 0.0f, 0.0f), vec4(0.9f, 0.25f, 0.2f, 1.0f)},
			{vec3(2, 1, 1), vec3(5, 0,  0), vec3(0.0f, 0.5f, 0.0f), vec4(0.2f, 0.8f, 0.3f, 1.0f)},
			{vec3(1, 2, 1), vec3(5, 0,  3), vec3(0.5f, 0.0f, 0.0f), vec4(0.2f, 0.4f, 0.95f, 1.0f)},
			{vec3(1, 1, 2), vec3(8, 2, -1), vec3(0.0f, 0.7f, 0.3f), vec4(0.95f, 0.75f, 0.15f, 1.0f)},
			{vec3(2, 1, 2), vec3(8,-2,  1), vec3(0.4f, 0.3f, 0.8f), vec4(0.75f, 0.25f, 0.85f, 1.0f)}
		};
		for(const TestObject& test_object : test_objects)
		{
			ConstructedObject* object = ConstructedObject::create(ShapeId::cube, test_object.size, test_object.color);
			if(object == nullptr)
			{
				continue;
			}
			object->pos = test_object.position;
			object->rotation = test_object.rotation;
			if(!map_root->addChild(object))
			{
				object->destroy();
			}
		}
	}

}
