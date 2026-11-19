#include <danikk_space_engine/material.h>

#include <danikk_framework/cstring_functions.h>

namespace danikk_space_engine
{
	namespace
	{
		DynamicArray<MaterialDefinition> material_list;
	}

	/**
	 * @brief Creates a material definition and decodes its packed RGB tint.
	 * @param name Stable material name used by getMaterialID().
	 * @param hexrgb_color Packed 0xRRGGBB color converted to normalized channel values.
	 * @param texture_name Texture asset name; null creates an empty texture for non-rendered material.
	 */
	MaterialDefinition::MaterialDefinition(const char* name, uint32 hexrgb_color, const char* texture_name)
		: name(name),
		  color(
			((hexrgb_color >> 16) & 0xFF) / 255.0f,
			((hexrgb_color >> 8) & 0xFF) / 255.0f,
			(hexrgb_color & 0xFF) / 255.0f
		  ),
		  texture(texture_name ? Texture(texture_name) : Texture())
	{}

	/**
	 * @brief Populates the process-wide material catalog once in the ID order declared by default_materials.
	 *
	 * The insertion sequence is part of the serialized/runtime ID contract: void, grass, stone,
	 * lava, water, magma, build_part, and dirt must stay aligned with the constants in `default_materials`.
	 * Repeated calls do nothing after the first population. Textured materials load their matching
	 * 32x32 assets; lava, water, and magma use the first frame of their source sprite sheets. The
	 * void and build-part entries have no texture. Grass and water carry explicit tint colors,
	 * while the remaining textured materials use white tint. Build parts use a plain gray tint.
	 */
	void loadMaterials()
	{

		if(material_list.size() != 0)
		{
			return;
		}

		material_list.pushCtor("void", 0x000000, nullptr);
		material_list.pushCtor("grass", 0x91BD59, "grass_32x32");
		material_list.pushCtor("stone", 0xFFFFFF, "stone_32x32");
		material_list.pushCtor("lava", 0xFFFFFF, "lava_32x32");
		material_list.pushCtor("water", 0x3F76E4, "water_32x32");
		material_list.pushCtor("magma", 0xFFFFFF, "magma_32x32");
		material_list.pushCtor("build_part", 0xA6ADB8, nullptr);
		material_list.pushCtor("dirt", 0xFFFFFF, "dirt_32x32");
	}

	/**
	 * @brief Performs a linear exact-name lookup in the material catalog.
	 * @param name Null-terminated material name.
	 * @return Catalog index for a match, or the void material ID when no name matches.
	 */
	MaterialId getMaterialID(const char* name)
	{
		for(index_t i = 0; i < material_list.size(); i++)
		{
			if(strequal(material_list[i].name, name))
			{
				return static_cast<MaterialId>(i);
			}
		}
		return default_materials::void_material;
	}

	/**
	 * @brief Returns the number of definitions currently stored in the material catalog.
	 * @return Material catalog length.
	 */
	size_t getMaterialCount()
	{
		return material_list.size();
	}

	/**
	 * @brief Returns a mutable material definition using its ID as a direct array index.
	 * @param id Valid index into material_list.
	 * @return Reference to the requested definition. No bounds check is performed here.
	 */
	MaterialDefinition& getMaterial(MaterialId id)
	{
		return material_list[id];
	}
}
