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
	struct StaticAssetCollection
	{
		Shader base2d_shader;
		Shader base3d_shader;
		Font base_font;
		Mesh low_poly_sphere_mesh;
		Mesh cube_mesh;
		Mesh mg42_mesh;
		HumanDefinition human_definition;
		// Skin 0 is the default coloured appearance; other entries are shared 64x64 textures.
		DynamicArray<Texture> minecraft_skins;

		Texture border_texture;
		Texture container_texture;

		void load();
		HumanSkinId loadMinecraftSkin(const char* texture_name);
		Texture& getMinecraftSkin(HumanSkinId skin_id);
	};

	/*class BlockAtlas
	{
		DynamicArray<Texture> array;
	public:
		static constexpr uint32 axis_size = 2048;
		static constexpr uint32 texture_axis_size = 128;
		static constexpr uint32 texture_axis_count = axis_size / texture_axis_size;

		Texture& getAtlas(uint index);

		Sprite getTextureFromAtlas(uint atlas_index, uint in_atlas_index);
	};*/

	//extern BlockAtlas block_atlas;
	extern StaticAssetCollection static_asset_collection;
}

