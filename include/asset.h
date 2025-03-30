#pragma once

#include <danikk_framework/dictionary.h>
#include <danikk_engine/texture.h>
#include <danikk_engine/font.h>
#include <danikk_engine/sprite.h>
#include <danikk_engine/shader.h>
#include <danikk_engine/style.h>
#include "default.h"

namespace game
{
	struct StaticAssetCollection
	{
		Shader base2d_shader;
		Shader base3d_shader;
		Font base_font;

		void load();
	};

	class BlockAtlas
	{
		DynamicArray<Texture> array;
	public:
		static constexpr uint32 axis_size = 2048;
		static constexpr uint32 texture_axis_size = 128;
		static constexpr uint32 texture_axis_count = axis_size / texture_axis_size;

		Texture& getAtlas(uint index);

		Sprite getTextureFromAtlas(uint atlas_index, uint in_atlas_index);
	};

	extern BlockAtlas block_atlas;
	extern StaticAssetCollection static_asset_collection;
}

