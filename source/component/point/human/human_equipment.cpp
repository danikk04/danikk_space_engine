#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine::human_internal
{
		/**
		 * @brief Draws equipped MG-42 items at the shoulder using the current weapon-ready pose.
		 * @param human_slot Human slot used to find inventory components on the same point.
		 * @param human Character pose, height, and head pitch.
		 * @param body_matrix Transform from character-local coordinates to render coordinates.
		 * @param world World used to inspect the owning chunk's component slots.
		 */
		void drawEquippedWeapons(const PointComponentSlot& human_slot, const HumanComponent& human,
			const mat4& body_matrix, World& world)
		{
			DSE_WATCH();
			if(human.arm_pose != HumanArmPose::weapon_ready) return;
			const Chunk& chunk = world.data.getChunk(human_slot.chunk_id);
			for(PointComponentSlotId component_slot_id : chunk.point_component_slot_ids)
			{
				if(!world.data.isPointComponentSlotActive(component_slot_id)) continue;
				const PointComponentSlot& component_slot = world.data.getPointComponentSlot(component_slot_id);
				if(component_slot.point_id != human_slot.point_id
					|| component_slot.type_id != PointComponentTypeId::inventory) continue;
				const InventoryComponent& inventory = *static_cast<const InventoryComponent*>(component_slot.getData());
				for(const InventorySlot& inventory_slot : inventory.slots)
				{
					if(!inventory_slot.is_equipped || inventory_slot.isEmpty()) continue;
					if(inventory_slot.item_definition_id != item_definition_ids::mg42) continue;

					mat4 weapon_orientation(1.0f);
					weapon_orientation[0] = vec4(0, 1, 0, 0);
					weapon_orientation[1] = vec4(0, 0, 1, 0);
					weapon_orientation[2] = vec4(1, 0, 0, 0);
					const vec3 shoulder_position = vec3(0.1875f, 0.0f, 0.25f) * human.getHeight();
					const vec3 attachment_offset = (vec3(0.11f, 0.37f, 0.22f)
						- vec3(0.1875f, 0.0f, 0.25f)) * human.getHeight();
					const mat4 weapon_pitch = glm::rotate(mat4(1.0f),
						glm::clamp(human.getHeadRotation().y, -0.8f, 0.8f), vec3(1, 0, 0));
					const mat4 weapon_yaw = glm::rotate(mat4(1.0f), human.getHeadRotation().x, vec3(0, 0, 1));
					const mat4 weapon_matrix = body_matrix
						* glm::translate(mat4(1.0f), shoulder_position)
						* weapon_yaw
						* weapon_pitch
						* glm::translate(mat4(1.0f), attachment_offset)
						* weapon_orientation
						* glm::scale(mat4(1.0f), vec3(0.25f));
					drawItem(inventory_slot.item_definition_id, weapon_matrix);
				}
				return;
			}
		}
}
