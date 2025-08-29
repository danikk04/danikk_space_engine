#pragma once

#include <default.h>

namespace danikk_space_engine
{
	struct Material
	{
		const char* name;
		Texture main_texture;

		Material(const char* name) : name(name){}
	};

	DynamicArray<Material> material_list
	{
		Material("sandstone"),
		Material("granite")
	};

	void initMaterials();
}
