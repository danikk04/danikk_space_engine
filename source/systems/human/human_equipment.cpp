#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/inventory.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/human/internal.h>

namespace danikk_space_engine::human_internal
{
		/**
		 * @brief Draws equipped MG-42 items at the shoulder using the current weapon-ready pose.
		 * @param inventory Human-owned inventory.
		 * @param human Character pose and height.
		 * @param body_matrix Transform from character-local coordinates to render coordinates.
		 */
		void drawEquippedWeapons(const InventoryComponent& inventory, const HumanState& human,
			const mat4& body_matrix)
		{
			DSE_WATCH();
			if(human.arm_pose != HumanArmPose::weapon_ready) return;
			for(const InventorySlot& inventory_slot : inventory.slots)
			{
				if(!inventory_slot.is_equipped || inventory_slot.isEmpty()) continue;
				if(inventory_slot.item_definition_id != item_definition_ids::mg42) continue;

				mat4 weapon_orientation(1.0f);
				weapon_orientation[0] = vec4(0, 1, 0, 0);
				weapon_orientation[1] = vec4(0, 0, 1, 0);
				weapon_orientation[2] = vec4(1, 0, 0, 0);
				const float shoulder_x = getHumanSkin(human.appearance.skin_id).model_type
					== HumanSkinModelType::slim ? 0.125f + 1.5f / 32.0f : 0.1875f;
				const vec3 shoulder_position = vec3(shoulder_x, 0.0f, 0.1875f) * human.getHeight();
				const vec3 attachment_offset = (vec3(0.11f, 0.37f, 0.22f)
					- vec3(0.1875f, 0.0f, 0.25f)) * human.getHeight();
				const mat4 weapon_pitch = glm::rotate(mat4(1.0f),
					human.getWeaponAimPitch(), vec3(1, 0, 0));
				const mat4 weapon_matrix = body_matrix
					* glm::translate(mat4(1.0f), shoulder_position)
					* weapon_pitch
					* glm::translate(mat4(1.0f), attachment_offset)
					* weapon_orientation
					* glm::scale(mat4(1.0f), vec3(0.25f));
				drawItem(inventory_slot.item_definition_id, weapon_matrix);
			}
		}
}
