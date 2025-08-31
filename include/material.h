#pragma once

#include <default.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	struct Material
	{
		const char* name;
		Texture main_texture;

		Material(const char* name)
		{
			this->name = name;
		}
	};

	uint32 getMaterialID(const char* name);

	Texture& getMaterialTexture(uint material_id);

	void initMaterials();
}
