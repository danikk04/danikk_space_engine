#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_engine/texture.h>

namespace danikk_space_engine
{
	/** @brief Built-in material IDs; each value is the corresponding material_list index. */
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
	 * @brief Defines the render tint and texture associated with a material ID.
	 *
	 * The catalog stores definitions in a DynamicArray, so each MaterialId is its
	 * current array index. The packed color constructor argument is decoded from
	 * 0xRRGGBB into normalized RGB channels. A null texture name creates an empty
	 * Texture; rendering code can then bind the white fallback texture while still
	 * applying this definition's tint.
	 */
	struct MaterialDefinition
	{
		const char* name;
		vec3 color;
		Texture texture;

		MaterialDefinition(const char* name, uint32 hexrgb_color, const char* texture_name);
	};

	/**
	 * @brief Loads the built-in void, grass, stone, lava, water, and magma definitions once.
	 *
	 * Repeated calls are no-ops after the first population. Insertion order defines
	 * the MaterialId values and must stay aligned with default_materials. Textures
	 * are loaded by asset name; the void material has no texture.
	 */
	void loadMaterials();

	/**
	 * @brief Finds a material by exact name and returns its array index.
	 * @param name Name to compare against the loaded material catalog.
	 * @return Matching MaterialId, or default_materials::void_material when absent.
	 */
	MaterialId getMaterialID(const char* name);

	/**
	 * @brief Returns the current number of loaded material definitions.
	 * @return Number of entries in the internal material array.
	 */
	size_t getMaterialCount();

	/**
	 * @brief Returns the catalog entry at an index-based material ID.
	 * @param id MaterialId used directly as an array index.
	 * @return Mutable reference to the definition. The ID must be less than
	 *         getMaterialCount(); this function does not perform a bounds check.
	 */
	MaterialDefinition& getMaterial(MaterialId id);
}
