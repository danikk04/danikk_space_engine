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
	 * copying GPU resources. Human skin textures are owned by the human-skin catalog.
	 */
	struct StaticAssetCollection
	{
		/** @brief Shared 2D GUI shader loaded during collection initialization. */
		Shader base2d_shader;
		/** @brief Shared 3D scene shader loaded during collection initialization. */
		Shader base3d_shader;
		/** @brief Unlit 3D shader used for bright projectile tracer quads. */
		Shader bullet_tracer_shader;
		/** @brief Shared bitmap font used by the default GUI style. */
		Font base_font;
		/** @brief Cube mesh resource slot; current tool previews use the world cube-mesh helper. */
		Mesh cube_mesh;
		/** @brief Shared human part geometry; character instances store only pose and appearance IDs. */
		HumanDefinition human_definition;
		/** @brief Shared UI border texture. */
		Texture border_texture;
		/** @brief Shared storage-container texture. */
		Texture container_texture;

		/**
		 * @brief Loads shared shaders, fonts, primitive meshes, item definitions, human geometry, and textures.
		 *
		 * Sets the engine's default GUI shader and font, loads the shared sphere,
		 * initializes item and human catalogs, then loads the container texture.
		 * Human skins are loaded independently by loadHumanSkins(). Call after
		 * graphics initialization and before creating objects that reference assets.
		 */
		void load();

	};

	/** @brief Process-wide shared asset collection used by rendering and item systems. */
	extern StaticAssetCollection static_asset_collection;
}

