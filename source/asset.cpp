#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/item.h>
#include <danikk_framework/static_string.h>
#include <danikk_engine/dynamic_mesh.h>

namespace danikk_space_engine
{
	StaticAssetCollection static_asset_collection;

	/**
	 * @brief Initializes global render assets and the item and human-definition catalogs.
	 *
	 * Must run after the graphics context and asset paths are initialized.
	 */
	void StaticAssetCollection::load()
	{
    	base2d_shader.load("base2d");
    	base3d_shader.load("base3d");
	    	base_font.load("archive");

    	setDefaultGUIShader(base2d_shader);
    	setDefaultFont(base_font);

    	low_poly_sphere_mesh = Mesh(NULL, "sphere");
		loadItemCatalog();
		loadHumanDefinition(human_definition);
    	container_texture = Texture("container");
	}
}
