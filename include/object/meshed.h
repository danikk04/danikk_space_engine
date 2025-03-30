#pragma once

#include <default.h>
#include <object/object.h>
#include <danikk_engine/mesh.h>

namespace game
{
	class MeshedObject : public virtual WorldObject
	{
	public:
		Mesh* mesh;
		Texture texture;
		vec4 color = vec4(1.0f);

		MeshedObject() : WorldObject()
		{

		}

		void frame() override
		{
			setWorldMatrix(world_matrix);
			setDrawColor(color);
			texture.bind();
			mesh->draw();
			Object::frame();
		}
	};
}
