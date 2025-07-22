#pragma once

#include <default.h>

namespace danikk_space_engine
{
	struct AtlasTextureLabel
	{
		char data[8];

		AtlasTextureLabel(const char* data)
		{
			strcpy(this->data, data);
		}
	};

	class BlockAtlas
	{
		struct Texture
		{
			danikk_engine::Texture texture;
			AtlasTextureLabel label;
		};

		DynamicArray<Texture> textures;

		void load();

		void loadTexture(const char* label);
	};
}
