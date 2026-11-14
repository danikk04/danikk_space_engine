#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	using MaterialId = uint32;

	namespace default_materials
	{
		constexpr inline MaterialId void_material = 0;
		constexpr inline MaterialId grass = 1;
		constexpr inline MaterialId stone = 2;
		constexpr inline MaterialId lava = 3;
		constexpr inline MaterialId water = 4;
	}

	struct MaterialDefinition
	{
		const char* name;
		vec3 color;
		Texture texture;

		MaterialDefinition(const char* name, uint32 hexrgb_color, const char* texture_name);
	};

	void loadMaterials();
	MaterialId getMaterialID(const char* name);
	MaterialDefinition& getMaterial(MaterialId id);
}
