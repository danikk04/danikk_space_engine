#include <cmath>

#include <danikk_framework/log.h>
#include <danikk_space_engine/component/point/character.h>
#include <danikk_space_engine/asset.h>

namespace danikk_space_engine
{
	namespace
	{
		// Draws the shared six-part model with a skin selected by ID. Joint rotations consume
		// the common pose state, while mesh dimensions and fallback colours belong to shared assets.
		void drawMinecraftCharacter(const CharacterComponent& character, const mat4& base, CharacterSkinId skin_id)
		{
			CharacterDefinition& definition = static_asset_collection.character_definition;
			Texture& skin = static_asset_collection.getMinecraftSkin(skin_id);
			const bool has_skin = !skin.isNull();
			if(has_skin) skin.bind();
			else white_texture.bind();
			const float swing = std::sin(character.walk_phase) * character.walk_amplitude;
			for(size_t index = 0; index < static_cast<size_t>(CharacterPart::count); index++)
			{
				CharacterPartDefinition& part = definition.parts[index];
				mat4 joint(1.0f);
				if(index == static_cast<size_t>(CharacterPart::head))
				{
					joint = glm::rotate(joint, character.head_rotation.x, vec3(0, 0, 1));
					joint = glm::rotate(joint, character.head_rotation.y, vec3(1, 0, 0));
				}
				else if(index >= 2)
				{
					const float angle = index == 2 || index == 5 ? swing : -swing;
					joint = glm::rotate(joint, angle, vec3(1, 0, 0));
				}
				setDrawColor(vec4(has_skin ? vec3(1.0f) : part.color, 1.0f));
				setWorldMatrix(base * glm::translate(mat4(1.0f), part.pivot) * joint
					* glm::translate(mat4(1.0f), part.center_offset) * glm::scale(mat4(1.0f), part.size));
				part.mesh.draw();
			}
		}
	}

	void drawCharacterAppearance(const CharacterComponent& character, const mat4& world_matrix)
	{
		switch(character.appearance.render_type)
		{
			case CharacterRenderType::minecraft_style:
				drawMinecraftCharacter(character, world_matrix, character.appearance.skin_id);
				return;
			case CharacterRenderType::model_3d:
			{
				// Model-specific IDs must not be interpreted as Minecraft texture indices.
				static bool reported_unimplemented = false;
				if(!reported_unimplemented)
				{
					formatLogWarning("3D character rendering is not implemented; using default Minecraft appearance");
					reported_unimplemented = true;
				}
				drawMinecraftCharacter(character, world_matrix, 0);
				return;
			}
		}
		formatFatalError("Unknown character render type %", static_cast<uint32>(character.appearance.render_type));
	}
}
