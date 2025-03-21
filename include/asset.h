#pragma once

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
		Texture container_texture;
		Texture error_texture;

		void load();
	};

	extern StaticAssetCollection static_asset_collection;
}

