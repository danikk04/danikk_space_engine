#pragma once

#include <default.h>
#include <object/gameobject.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class ConstructedObject : public GameObject
	{
	public:
		Texture texture;
		vec4 color = vec4(1.0f);

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
