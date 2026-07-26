#pragma once

#include <default.h>
#include <object/object.h>
#include <danikk_engine/mesh.h>
#include "posed.h"

namespace danikk_space_engine
{
	class MeshedObject : public PosedObject
	{
	public:
		Mesh* mesh;
		Texture texture;
		vec4 color = vec4(1.0f);

		void frame()
		{
			setWorldMatrix(world_matrix);
			setDrawColor(color);
			texture.bind();
			mesh->draw();
		}
	};
}
