#include <danikk_space_engine/material.h>

#include <danikk_framework/cstring_functions.h>

namespace danikk_space_engine
{
	namespace
	{
		DynamicArray<MaterialDefinition> material_list;
	}

	MaterialDefinition::MaterialDefinition(const char* name, uint32 hexrgb_color, const char* texture_name)
		: name(name),
		  color(
			((hexrgb_color >> 16) & 0xFF) / 255.0f,
			((hexrgb_color >> 8) & 0xFF) / 255.0f,
			(hexrgb_color & 0xFF) / 255.0f
		  ),
		  texture(texture_name ? Texture(texture_name) : Texture())
	{}

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
	}

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

	size_t getMaterialCount()
	{
		return material_list.size();
	}

	MaterialDefinition& getMaterial(MaterialId id)
	{
		return material_list[id];
	}
}
