#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/object/gameobject.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class ConstructedObject : public GameObject
	{
		Material* main_material;
	public:
		void render() override
		{
			GameObject::render();
			setWorldMatrix(world_matrix);
			setDrawColor(main_material->color);
			texture.bind();
			mesh->draw();
		}
	};
}
