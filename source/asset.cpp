#include <asset.h>

namespace sotiria_project
{
	StaticAssetCollection static_asset_collection;

	void StaticAssetCollection::load()
	{
    	base_shader.load("base2d");
    	container_sprite = Texture("container").createSprite(0.5f, 0.5f, 0.5f, 0.5f);
    	base_font.load("archive", FontTypes::rasterized);

    	setDefaultGUIShader(base_shader);
    	setDefaultFont(base_font);
	}
}
