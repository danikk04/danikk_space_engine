#pragma once

#include <default.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	struct Material
	{
		const char* name;
		vec3 color;

		Material(const char* name, uint32 hexrgb_color)
		{
			this->name = name;
			color = vec3(
				((hexrgb_color >> 16) & 0xFF) / 255.0f,  // Красный канал (R)
				((hexrgb_color >> 8)  & 0xFF) / 255.0f,  // Зеленый канал (G)
				((hexrgb_color)       & 0xFF) / 255.0f   // Синий канал (B)
			);
		}
	};

	namespace default_materials
	{
#define mat(name, id) constexpr inline uint32 name ## id = id;
	mat(void, 0)
	mat(sandstone, 1)
	mat(granite, 2)
#undef mat
	}

	uint32 getMaterialID(const char* name);

	void loadMaterials();
}
