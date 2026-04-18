#pragma once

#include <default.h>
#include <object/object.h>
#include <object/world.h>
#include <danikk_engine/mesh.h>

namespace danikk_space_engine
{
	namespace object_tags
	{
		class Meshed : public World
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
}
