#pragma once

#include <default.h>
#include <objects/object.h>
#include <danikk_engine/mesh.h>

namespace sotiria_project
{
	class MeshedObject : virtual WorldObject
	{
		Mesh mesh;
		Texture texture;
		vec4 color = vec4(1.0f);

		void frame(Object& parent) override
		{
			setWorldMatrix(world_matrix);
			setDrawColor(color);
			uint texture_handle = !texture.isNull() ? texture.container->texture_data.handle : 0;
			bindTexture(texture_handle, 0);
			mesh.draw();
			Object::frame(parent);
		}
	};
}
