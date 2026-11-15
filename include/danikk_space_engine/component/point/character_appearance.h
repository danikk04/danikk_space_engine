#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	using CharacterSkinId = uint32;

	enum class CharacterRenderType : uint8
	{
		minecraft_style,
		model_3d
	};

	struct CharacterAppearance
	{
		CharacterRenderType render_type = CharacterRenderType::minecraft_style;
		CharacterSkinId skin_id = 0;
	};

	struct CharacterComponent;
	void drawCharacterAppearance(const CharacterComponent& character, const mat4& world_matrix);
}
