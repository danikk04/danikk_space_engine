#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/item/definition.h>
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
		planet_terrain_shader.load("planet_terrain");
		bullet_tracer_shader.load("bullet_tracer");
	    	base_font.load("archive");

    	setDefaultGUIShader(base2d_shader);
    	setDefaultFont(base_font);

		loadItemCatalog();
		loadHumanDefinition(human_definition);
    	container_texture = Texture("container");
	}
}
