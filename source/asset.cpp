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
	 * The skin array is cleared and seeded with an empty entry at ID zero before
	 * the bundled soldier texture is appended. This keeps the fallback skin ID
	 * stable and lets unspecialized humans render with per-part colors. Must run
	 * after the graphics context and asset paths are initialized.
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
		minecraft_skins.clear();
		minecraft_skins.pushCtor();
		usmc_marpat_woodland_skin_id = loadMinecraftSkin(
			"minecraft_skins/medium-loadout-marpat-woodland-14906056-e1810");
    	container_texture = Texture("container");
	}

	/**
	 * @brief Appends a nearest-filtered skin texture and returns its index in minecraft_skins.
	 * @param texture_name Texture asset name relative to the data texture directory, without extension.
	 * @return ID assigned to the appended texture; if called before load(), an
	 *         empty fallback entry is inserted first so zero remains reserved.
	 * The image is assumed to use the 64x64 Minecraft skin layout; its dimensions
	 * are not checked here. Appending may reallocate the array and invalidate references
	 * previously returned by getMinecraftSkin().
	 */
	HumanSkinId StaticAssetCollection::loadMinecraftSkin(const char* texture_name)
	{
		if(minecraft_skins.size() == 0) minecraft_skins.pushCtor();
		const HumanSkinId id = static_cast<HumanSkinId>(minecraft_skins.size());
		minecraft_skins.pushCtor(texture_name, texture_filters::nearest);
		return id;
	}

	/**
	 * @brief Returns the shared Minecraft skin texture after validating its catalog index.
	 * @param skin_id Previously assigned skin catalog ID.
	 * @return Mutable reference to the shared texture; invalid IDs trigger a fatal error.
	 * The reference points into the owning DynamicArray and must not be retained
	 * across operations that append another skin.
	 */
	Texture& StaticAssetCollection::getMinecraftSkin(HumanSkinId skin_id)
	{
		if(skin_id >= minecraft_skins.size()) formatFatalError("Invalid Minecraft human skin ID %", skin_id);
		return minecraft_skins[skin_id];
	}

}
