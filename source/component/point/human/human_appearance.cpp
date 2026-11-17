#include <cmath>

#include <danikk_framework/log.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/asset.h>

namespace danikk_space_engine
{
	namespace
	{
		/**
		 * @brief Renders six posed body-part meshes and, when present, their Minecraft outer skin layer.
		 *
		 * Base meshes use per-part atlas UVs and either the skin texture or fallback part colors.
		 * Overlay meshes use the second atlas layer, are scaled by 1.04, and render with depth writes
		 * disabled so transparent texels reveal the base layer.
		 * @param human Current walk cycle, head rotation, and arm pose.
		 * @param base Character transform shared by all six parts.
		 * @param skin_id Index into the loaded Minecraft skin texture array; index zero is fallback.
		 */
		void drawMinecraftHuman(const HumanComponent& human, const mat4& base, HumanSkinId skin_id)
		{
			DSE_WATCH();
			HumanDefinition& definition = static_asset_collection.human_definition;
			Texture& skin = static_asset_collection.getMinecraftSkin(skin_id);
			const bool has_skin = !skin.isNull();
			if(has_skin) skin.bind();
			else white_texture.bind();
			const float swing = std::sin(human.walk_phase) * human.walk_amplitude;
			const float weapon_arm_pitch = 1.570796327f + glm::clamp(human.getHeadRotation().y, -0.8f, 0.8f);
			mat4 joint_matrices[static_cast<size_t>(HumanPart::count)];
			for(size_t index = 0; index < static_cast<size_t>(HumanPart::count); index++)
			{
				HumanPartDefinition& part = definition.parts[index];
				mat4 joint(1.0f);
				if(index == static_cast<size_t>(HumanPart::head))
				{
					joint = glm::rotate(joint, human.getHeadRotation().x, vec3(0, 0, 1));
					joint = glm::rotate(joint, human.getHeadRotation().y, vec3(1, 0, 0));
				}
				else if(index >= 2)
				{
					float angle = index == 2 || index == 5 ? swing : -swing;
					if(human.arm_pose == HumanArmPose::weapon_ready)
					{
						if(index == static_cast<size_t>(HumanPart::right_arm))
							angle = weapon_arm_pitch;
						else if(index == static_cast<size_t>(HumanPart::left_arm))
						{
							/** @brief Rotates the left arm inward across the chest and raises it toward the weapon grip. */
							joint = glm::rotate(joint, -0.610865238f, vec3(0, 0, 1));
							angle = weapon_arm_pitch;
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

	/**
	 * @brief Dispatches appearance rendering; unsupported full-model rendering warns once and falls back.
	 * @param human Appearance selection and animated pose to render.
	 * @param world_matrix Character-local to render-space transform.
	 */
	void drawHumanAppearance(const HumanComponent& human, const mat4& world_matrix)
	{
		DSE_WATCH();
		switch(human.appearance.render_type)
		{
			case HumanRenderType::minecraft_style:
				drawMinecraftHuman(human, world_matrix, human.appearance.skin_id);
				return;
			case HumanRenderType::model_3d:
			{
				/** @brief Keep 3D model IDs separate from indices into the Minecraft skin texture table. */
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
