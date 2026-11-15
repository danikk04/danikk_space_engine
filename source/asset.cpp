#include <danikk_space_engine/asset.h>
#include <danikk_framework/static_string.h>
#include <danikk_engine/dynamic_mesh.h>

namespace danikk_space_engine
{
	StaticAssetCollection static_asset_collection;

	void StaticAssetCollection::load()
	{
    	base2d_shader.load("base2d");
    	base3d_shader.load("base3d");
	    	base_font.load("archive");

    	setDefaultGUIShader(base2d_shader);
    	setDefaultFont(base_font);

    	low_poly_sphere_mesh = Mesh(NULL, "sphere");
		mg42_mesh.loadObj("MG-42", "MG-42");
		loadHumanDefinition(human_definition);
		minecraft_skins.clear();
		minecraft_skins.pushCtor();
    	container_texture = Texture("container");
	}

	HumanSkinId StaticAssetCollection::loadMinecraftSkin(const char* texture_name)
	{
		if(minecraft_skins.size() == 0) minecraft_skins.pushCtor();
		const HumanSkinId id = static_cast<HumanSkinId>(minecraft_skins.size());
		minecraft_skins.pushCtor(texture_name, texture_filters::nearest);
		return id;
	}

	Texture& StaticAssetCollection::getMinecraftSkin(HumanSkinId skin_id)
	{
		if(skin_id >= minecraft_skins.size()) formatFatalError("Invalid Minecraft human skin ID %", skin_id);
		return minecraft_skins[skin_id];
	}

	/*Texture& BlockAtlas::getAtlas(uint index)
	{
		if(array.size() < index)
		{
			array.rawResize(index - array.size() + 1);
			array[index].container = NULL;
		}
		if(array[index].isNull())
		{
			StringBuffer<10> atlas_name;
			atlas_name << "atlas" << (size_t)index;

			array[index] = Texture(atlas_name.c_string());
		}
		return array[index];
	}

	Sprite BlockAtlas::getTextureFromAtlas(uint atlas_index, uint in_atlas_index)
	{
		Texture& texture = getAtlas(atlas_index);
		return texture.createSpriteByIndex(in_atlas_index, texture_axis_size, texture_axis_count);
	}*/
}
