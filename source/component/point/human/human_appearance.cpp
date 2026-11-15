#include <cmath>

#include <danikk_framework/log.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/asset.h>

namespace danikk_space_engine
{
	namespace
	{
		// Draws the base body meshes first, then the UV-mapped outer skin layer with a slight shell
		// expansion. The overlay pass disables depth writes so transparent pixels reveal the base.
		void drawMinecraftHuman(const HumanComponent& human, const mat4& base, HumanSkinId skin_id)
		{
			HumanDefinition& definition = static_asset_collection.human_definition;
			Texture& skin = static_asset_collection.getMinecraftSkin(skin_id);
			const bool has_skin = !skin.isNull();
			if(has_skin) skin.bind();
			else white_texture.bind();
			const float swing = std::sin(human.walk_phase) * human.walk_amplitude;
			mat4 joint_matrices[static_cast<size_t>(HumanPart::count)];
			for(size_t index = 0; index < static_cast<size_t>(HumanPart::count); index++)
			{
				HumanPartDefinition& part = definition.parts[index];
				mat4 joint(1.0f);
				if(index == static_cast<size_t>(HumanPart::head))
				{
					joint = glm::rotate(joint, human.head_rotation.x, vec3(0, 0, 1));
					joint = glm::rotate(joint, human.head_rotation.y, vec3(1, 0, 0));
				}
				else if(index >= 2)
				{
					float angle = index == 2 || index == 5 ? swing : -swing;
					if(human.arm_pose == HumanArmPose::weapon_ready)
					{
						if(index == static_cast<size_t>(HumanPart::right_arm))
							angle = 1.570796327f;
						else if(index == static_cast<size_t>(HumanPart::left_arm))
						{
							// Turn the left arm inward across the chest, then raise it toward the weapon.
							joint = glm::rotate(joint, -0.610865238f, vec3(0, 0, 1));
							angle = 1.570796327f;
						}
					}
					joint = glm::rotate(joint, angle, vec3(1, 0, 0));
				}
				joint_matrices[index] = joint;
				setDrawColor(vec4(has_skin ? vec3(1.0f) : part.color, 1.0f));
				setWorldMatrix(base * glm::translate(mat4(1.0f), part.pivot) * joint
					* glm::translate(mat4(1.0f), part.center_offset) * glm::scale(mat4(1.0f), part.size));
				part.mesh.draw();
			}

			if(!has_skin) return;
			setDepthWriteEnabled(false);
			for(size_t index = 0; index < static_cast<size_t>(HumanPart::count); index++)
			{
				HumanPartDefinition& part = definition.parts[index];
				setDrawColor(vec4(1.0f));
				setWorldMatrix(base * glm::translate(mat4(1.0f), part.pivot) * joint_matrices[index]
					* glm::translate(mat4(1.0f), part.center_offset)
					* glm::scale(mat4(1.0f), part.size * 1.04f));
				part.overlay_mesh.draw();
			}
			setDepthWriteEnabled(true);
		}
	}

	void drawHumanAppearance(const HumanComponent& human, const mat4& world_matrix)
	{
		switch(human.appearance.render_type)
		{
			case HumanRenderType::minecraft_style:
				drawMinecraftHuman(human, world_matrix, human.appearance.skin_id);
				return;
			case HumanRenderType::model_3d:
			{
				// Model-specific IDs must not be interpreted as Minecraft texture indices.
				static bool reported_unimplemented = false;
				if(!reported_unimplemented)
				{
					formatLogWarning("3D human rendering is not implemented; using default Minecraft appearance");
					reported_unimplemented = true;
				}
				drawMinecraftHuman(human, world_matrix, 0);
				return;
			}
		}
		formatFatalError("Unknown human render type %", static_cast<uint32>(human.appearance.render_type));
	}
}
