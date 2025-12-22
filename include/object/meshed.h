#pragma once

#include <default.h>
#include <object/object.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	class MeshedObject
	{
		static uint32 id;
	public:
		Mesh* mesh;
		Texture texture;
		vec4 color = vec4(1.0f);

		void frame()
		{
			object_stack.peek().
			setWorldMatrix(world_matrix);
			setDrawColor(color);
			texture.bind();
			mesh->draw();
		}
	};
}
