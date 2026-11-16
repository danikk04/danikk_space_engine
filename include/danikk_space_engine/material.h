#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	namespace default_materials
	{
		constexpr inline MaterialId void_material = 0;
		constexpr inline MaterialId grass = 1;
		constexpr inline MaterialId stone = 2;
		constexpr inline MaterialId lava = 3;
		constexpr inline MaterialId water = 4;
		constexpr inline MaterialId magma = 5;
	}

	/**
	 * @brief Associates a material name and display color with its texture asset.
	 */
	struct MaterialDefinition
	{
		const char* name;
		vec3 color;
		Texture texture;

		MaterialDefinition(const char* name, uint32 hexrgb_color, const char* texture_name);
	};

	/**
	 * @brief Loads materials.
	 */
	void loadMaterials();

	/**
	 * @brief Looks up a material name and returns its table index, falling back to the default material when absent.
	 */
	MaterialId getMaterialID(const char* name);

	/**
	 * @brief Returns the number of entries in the material definition table.
	 */
	size_t getMaterialCount();

	/**
	 * @brief Returns the material at an ID after checking that the ID is in range.
	 */
	MaterialDefinition& getMaterial(MaterialId id);
}
