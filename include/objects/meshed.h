#pragma once

#include <default.h>
#include <objects/object.h>
#include <danikk_engine/mesh.h>

namespace sotiria_project
{
	class MeshedObject : virtual Object
	{
		Mesh mesh;
		Texture texture;

		void frame(Object& parent) override
		{
			glUniformMatrix4fv(shader_layout_locations::world, 1, 0, (float*)&world);
			glUniformMatrix2fv(shader_layout_locations::uv, 1, 0, (float*)&uv);
			glUniform4fv(shader_layout_locations::color, 1, (float*)&color);
			texture = texture ? texture : white_texture.container->texture_data.handle;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
			mesh.bind();
			mesh.draw();
			Object::frame(parent);
		}
	};
}
