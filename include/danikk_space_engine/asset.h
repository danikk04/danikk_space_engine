#pragma once

#include <danikk_framework/dictionary.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/font.h>
#include <danikk_engine/sprite.h>
#include <danikk_engine/shader.h>
#include <danikk_engine/style.h>
#include <danikk_engine/mesh.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns shared shaders, fonts, meshes, textures, and human appearance assets.
	 *
	 * This collection is initialized once after graphics setup. Its textures and meshes are shared
	 * by world components; components retain catalog IDs or use the collection directly rather than
	 * copying GPU resources. Skin IDs are array indices and remain valid only until the catalog is
	 * cleared/reloaded or its ordering changes. References returned by getMinecraftSkin() can also
	 * be invalidated when another skin is appended.
	 */
	struct StaticAssetCollection
	{
		/** @brief Shared 2D GUI shader loaded during collection initialization. */
		Shader base2d_shader;
		/** @brief Shared 3D scene shader loaded during collection initialization. */
		Shader base3d_shader;
		/** @brief Shared bitmap font used by the default GUI style. */
		Font base_font;
		/** @brief Low-poly sphere mesh reused by bullet tracers and primitive previews. */
		Mesh low_poly_sphere_mesh;
		/** @brief Cube mesh resource slot; current tool previews use the world cube-mesh helper. */
		Mesh cube_mesh;
		/** @brief Shared human part geometry; character instances store only pose and appearance IDs. */
		HumanDefinition human_definition;
		/** @brief Shared Minecraft skin textures; index zero is an empty-texture fallback entry. */
		DynamicArray<Texture> minecraft_skins;
		/** @brief Catalog ID assigned to the bundled woodland soldier skin during load(). */
		HumanSkinId usmc_marpat_woodland_skin_id = 0;

		/** @brief Shared UI border texture. */
		Texture border_texture;
		/** @brief Shared storage-container texture. */
		Texture container_texture;

		/**
		 * @brief Loads shared shaders, fonts, primitive meshes, item definitions, human geometry, and textures.
		 *
		 * Sets the engine's default GUI shader and font, loads the shared sphere,
		 * initializes item and human catalogs, resets the skin table, reserves
		 * index zero as the no-skin fallback, then loads the bundled woodland skin
		 * and container texture. Call after graphics initialization and before
		 * creating objects that reference these assets. Reloading clears the skin
		 * array and invalidates references into its previous contents.
		 */
		void load();

		/**
		 * @brief Loads a Minecraft skin texture with nearest-neighbor filtering and appends it to the catalog.
		 * @param texture_name Asset path relative to the texture data directory, without the `.png` extension.
		 * @return Index of the newly appended texture; zero is reserved for the fallback entry.
		 * @note The renderer expects a 64x64 Minecraft skin layout, but this function does not validate image dimensions.
		 */
		HumanSkinId loadMinecraftSkin(const char* texture_name);

		/**
		 * @brief Returns a loaded skin texture by its catalog ID.
		 * @param skin_id Index previously returned by loadMinecraftSkin(), or zero for fallback.
		 * @return Mutable reference to the shared texture; an out-of-range ID causes a fatal error.
		 * The returned reference is owned by this collection and can be invalidated
		 * if the skin array is later reallocated.
		 */
		Texture& getMinecraftSkin(HumanSkinId skin_id);
	};

	/** @brief Process-wide shared asset collection used by rendering and item systems. */
	extern StaticAssetCollection static_asset_collection;
}

