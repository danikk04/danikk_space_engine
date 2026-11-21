#include <cmath>

#include <danikk_framework/log.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/assets/collection.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr float minecraft_skin_pixel_size = 1.0f / 32.0f;

		/**
		 * @brief Renders six posed body-part meshes and, when present, their Minecraft outer skin layer.
		 *
		 * Base meshes use per-part atlas UVs and either the skin texture or fallback part colors.
		 * Overlay meshes use the second atlas layer and extend one skin pixel beyond each base face.
		 * Alpha discard preserves transparent texels while depth writes keep overlapping shell faces
		 * correctly ordered.
		 * @param human Current walk cycle and arm pose.
		 * @param base Character transform shared by all six parts.
		 * @param skin_id Index into the loaded Minecraft skin texture array; index zero is fallback.
		 */
		void drawMinecraftHuman(const HumanComponent& human, const mat4& base, HumanSkinId skin_id,
			const vec4& color_tint)
		{
			DSE_WATCH();
			HumanDefinition& definition = static_asset_collection.human_definition;
			HumanSkin& human_skin = getHumanSkin(skin_id);
			Texture& skin = human_skin.texture;
			const bool slim = human_skin.model_type == HumanSkinModelType::slim;
			const bool has_skin = !skin.isNull();
			if(has_skin) skin.bind();
			else white_texture.bind();
			const float swing = std::sin(human.walk_phase) * human.walk_amplitude;
		const float weapon_arm_pitch = 1.570796327f + glm::clamp(human.getWeaponAimPitch(), -0.8f, 0.8f);
			mat4 joint_matrices[static_cast<size_t>(HumanPart::count)];
			for(size_t index = 0; index < static_cast<size_t>(HumanPart::count); index++)
			{
				HumanPartDefinition& part = definition.parts[index];
				const bool slim_arm = slim && (index == static_cast<size_t>(HumanPart::right_arm)
					|| index == static_cast<size_t>(HumanPart::left_arm));
				vec3 part_pivot = part.pivot;
				vec3 part_size = part.size;
				if(slim_arm)
				{
					part_pivot.x = index == static_cast<size_t>(HumanPart::right_arm)
						? 0.125f + 1.5f / 32.0f : -0.125f - 1.5f / 32.0f;
					part_size.x = 3.0f / 32.0f;
				}
				mat4 joint(1.0f);
				if(index == static_cast<size_t>(HumanPart::head))
				{
					joint = glm::rotate(joint, human.getHeadPitch(), vec3(1.0f, 0.0f, 0.0f));
				}
				else if(index >= 2)
				{
					float angle = index == 2 || index == 5 ? swing : -swing;
					if(human.arm_pose == HumanArmPose::corpse_spread)
					{
						if(index == static_cast<size_t>(HumanPart::right_arm))
						{
							joint = glm::rotate(joint, -human.corpse_arm_spread_angles.x, vec3(0, 1, 0));
							angle = 0.0f;
						}
						else if(index == static_cast<size_t>(HumanPart::left_arm))
						{
							joint = glm::rotate(joint, human.corpse_arm_spread_angles.y, vec3(0, 1, 0));
							angle = 0.0f;
						}
					}
					else if(human.arm_pose == HumanArmPose::weapon_ready)
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
				setDrawColor(vec4((has_skin ? vec3(1.0f) : part.color) * vec3(color_tint), color_tint.a));
				setWorldMatrix(base * glm::translate(mat4(1.0f), part_pivot) * joint
					* glm::translate(mat4(1.0f), part.center_offset) * glm::scale(mat4(1.0f), part_size));
				(slim_arm ? part.slim_mesh : part.mesh).draw();
			}

			if(!has_skin) return;
			setDepthWriteEnabled(true);
			setAlphaDiscardEnabled(true);
			setFaceCullingEnabled(false);
			for(size_t index = 0; index < static_cast<size_t>(HumanPart::count); index++)
			{
				HumanPartDefinition& part = definition.parts[index];
				const bool slim_arm = slim && (index == static_cast<size_t>(HumanPart::right_arm)
					|| index == static_cast<size_t>(HumanPart::left_arm));
				vec3 part_pivot = part.pivot;
				vec3 part_size = part.size;
				if(slim_arm)
				{
					part_pivot.x = index == static_cast<size_t>(HumanPart::right_arm)
						? 0.125f + 1.5f / 32.0f : -0.125f - 1.5f / 32.0f;
					part_size.x = 3.0f / 32.0f;
				}
				setDrawColor(color_tint);
				setWorldMatrix(base * glm::translate(mat4(1.0f), part_pivot) * joint_matrices[index]
					* glm::translate(mat4(1.0f), part.center_offset)
					* glm::scale(mat4(1.0f), part_size + vec3(2.0f * minecraft_skin_pixel_size)));
				(slim_arm ? part.slim_overlay_mesh : part.overlay_mesh).draw();
			}
			setFaceCullingEnabled(true);
			setAlphaDiscardEnabled(false);
			setDepthWriteEnabled(true);
		}
	}

	/**
	 * @brief Dispatches appearance rendering; unsupported full-model rendering warns once and falls back.
	 * @param human Appearance selection and animated pose to render.
	 * @param world_matrix Character-local to render-space transform.
	 */
	void drawHumanAppearance(const HumanComponent& human, const mat4& world_matrix, const vec4& color_tint)
	{
		DSE_WATCH();
		switch(human.appearance.render_type)
		{
			case HumanRenderType::minecraft_style:
				drawMinecraftHuman(human, world_matrix, human.appearance.skin_id, color_tint);
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
				drawMinecraftHuman(human, world_matrix, human_skin_ids::none, color_tint);
				return;
			}
		}
		formatFatalError("Unknown human render type %", static_cast<uint32>(human.appearance.render_type));
	}
}
