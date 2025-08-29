#pragma once

#include <material.h>

namespace danikk_space_engine
{
	void initMaterials()
	{
		for(Material& material : material_list)
		{
			material.main_texture = Texture(material.name);
		}
	}
}
