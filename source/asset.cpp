#include <asset.h>

namespace game
{
	StaticAssetCollection static_asset_collection;

	void StaticAssetCollection::load()
	{
    	base2d_shader.load("base2d");
    	base3d_shader.load("base3d");
    	container_texture = Texture("container");
    	base_font.load("archive", FontTypes::rasterized);

    	setDefaultGUIShader(base2d_shader);
    	setDefaultFont(base_font);
	}
}
