#include <asset.h>
#include <danikk_framework/string_buffer.h>

namespace game
{
	StaticAssetCollection static_asset_collection;
	BlockAtlas block_atlas;

	void StaticAssetCollection::load()
	{
    	base2d_shader.load("base2d");
    	base3d_shader.load("base3d");
    	base_font.load("archive", FontTypes::rasterized);

    	setDefaultGUIShader(base2d_shader);
    	setDefaultFont(base_font);
	}

	Texture& BlockAtlas::getAtlas(uint index)
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
	}
}
