#pragma once

#include <default.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	struct Material
	{
		const char* name;
		vec3 color;

		Material(const char* name)
		{
			this->name = name;
		}
	};

	uint32 getMaterialID(const char* name);

	void initMaterials();
}
