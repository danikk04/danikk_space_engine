#pragma once

#include <default.h>
#include <material.h>
#include <object/gameobject.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class ConstructedObject : public GameObject
	{
		Material* main_material;
	public:
		void frame()
		{
			GameObject::frame();
			setWorldMatrix(world_matrix);
			setDrawColor(color);
			texture.bind();
			mesh->draw();
		}
	};
}
