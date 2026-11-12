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
		material_list.pushCtor("sandstone", 0xC4A484, "sandstone");
		material_list.pushCtor("granite", 0x746A5E, "granite");
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

	MaterialDefinition& getMaterial(MaterialId id)
	{
		return material_list[id];
	}
}
