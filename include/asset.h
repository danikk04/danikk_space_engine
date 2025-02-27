#pragma once

#include <danikk_engine/texture.h>
#include <danikk_engine/font.h>
#include <danikk_engine/sprite.h>
#include <danikk_engine/shader.h>
#include <danikk_engine/style.h>
#include "default.h"

namespace sotiria_project
{
	struct StaticAssetCollection
	{
		Shader base_shader;
		Font base_font;
		Sprite container_sprite;

		void load();
	};

	extern StaticAssetCollection static_asset_collection;
}

