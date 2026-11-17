#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine::npc_internal
{
		/** @brief Checks one candidate per tick, rotates targets each second and fires at the active target. */
		bool updateCombat(World& world, PointId point_id, ChunkId chunk_id,
			NpcControllerComponent& controller, HumanComponent& human, float dt)
		{
			DSE_WATCH();
			const Chunk& own_chunk = world.data.getChunk(chunk_id);
			const glm::ivec3 own_coordinate = glm::ivec3(glm::floor(
				(own_chunk.position + glm::dvec3(world.data.getPoint(point_id).position)) / planet_chunk_width));
			auto nearbyEnemy = [&](const NpcTarget& target)
			{
				if(!validEnemy(world, target, human.faction_id)) return false;
				const PointComponentSlot& other = world.data.getPointComponentSlot(target.slot_id);
				const Chunk& other_chunk = world.data.getChunk(other.chunk_id);
				if(other_chunk.world_body_id != own_chunk.world_body_id) return false;
				const glm::ivec3 other_coordinate = glm::ivec3(glm::floor(
					(other_chunk.position + glm::dvec3(world.data.getPoint(other.point_id).position)) / planet_chunk_width));
				const glm::ivec3 difference = glm::abs(other_coordinate - own_coordinate);
				return difference.x <= 1 && difference.y <= 1 && difference.z <= 1;
			};
			for(NpcTarget& target : controller.targeted_enemy)
			{
				target.visibility_age += dt;
				if(!nearbyEnemy(target) || target.visibility_age > 2.0f) target = NpcTarget{};
			}
			if(controller.candidate_index >= controller.enemy_candidates.size())
				collectCandidates(world, point_id, chunk_id, human.faction_id, controller);

			const Chunk& chunk = world.data.getChunk(chunk_id);
			const glm::dquat rotation = world.data.getWorldBody(chunk.world_body_id).rotation;
			const glm::dvec3 origin = world.getPointWorldPosition(chunk_id, point_id);
			const glm::dvec3 shot_origin = origin + rotation * glm::dvec3(human.up * (human.getHeight() * 0.2f));
			if(controller.candidate_index < controller.enemy_candidates.size())
			{
				const NpcTarget candidate = controller.enemy_candidates[controller.candidate_index++];
				if(nearbyEnemy(candidate))
				{
					const PointComponentSlot& target_slot = world.data.getPointComponentSlot(candidate.slot_id);
					const glm::dvec3 delta = world.getPointWorldPosition(target_slot.chunk_id, candidate.point_id) - shot_origin;
					const double distance = glm::length(delta);
					bool visible = false;
					if(distance > 0.001)
					{
						const RaycastHit hit = world.raycast(shot_origin, vec3(delta), static_cast<float>(distance));
						visible = !hit.hit || hit.distance >= distance - 0.1;
					}
					int existing = -1;
					int free_index = -1;
					for(int i = 0; i < 3; ++i)
					{
						if(controller.targeted_enemy[i].slot_id == candidate.slot_id && controller.targeted_enemy[i].point_id == candidate.point_id) existing = i;
						if(controller.targeted_enemy[i].slot_id == static_cast<PointComponentSlotId>(-1) && free_index < 0) free_index = i;
					}
					if(!visible && existing >= 0) controller.targeted_enemy[existing] = NpcTarget{};
					if(visible && existing >= 0) controller.targeted_enemy[existing].visibility_age = 0.0f;
					if(visible && existing < 0)
					{
						if(free_index < 0)
						{
							double farthest = glm::dot(delta, delta);
							for(int i = 0; i < 3; ++i)
							{
								const NpcTarget& current = controller.targeted_enemy[i];
								const PointComponentSlot& current_slot = world.data.getPointComponentSlot(current.slot_id);
								const glm::dvec3 offset = world.getPointWorldPosition(current_slot.chunk_id, current.point_id) - shot_origin;
								const double distance_squared = glm::dot(offset, offset);
								if(distance_squared > farthest) { farthest = distance_squared; free_index = i; }
							}
						}
						if(free_index >= 0) controller.targeted_enemy[free_index] = candidate;
					}
				}
			}
			controller.target_switch_timer += dt;
			while(controller.target_switch_timer >= 1.0f)
			{
				controller.target_switch_timer -= 1.0f;
				for(int i = 0; i < 3; ++i)
				{
					controller.target_index = (controller.target_index + 1) % 3;
					if(controller.targeted_enemy[controller.target_index].slot_id != static_cast<PointComponentSlotId>(-1)) break;
				}
			}
			if(controller.targeted_enemy[controller.target_index].slot_id == static_cast<PointComponentSlotId>(-1))
				for(uint8 i = 0; i < 3; ++i)
					if(controller.targeted_enemy[i].slot_id != static_cast<PointComponentSlotId>(-1)) { controller.target_index = i; break; }
			const NpcTarget target = controller.targeted_enemy[controller.target_index];
			if(!nearbyEnemy(target)) return false;
			const PointComponentSlot& target_slot = world.data.getPointComponentSlot(target.slot_id);
			const glm::dvec3 target_position = world.getPointWorldPosition(target_slot.chunk_id, target.point_id);
			const vec3 direction = vec3(glm::conjugate(rotation) * (target_position - shot_origin));
			const float length = glm::length(direction);
			if(length < 0.001f) return false;
			human.lookAtDirection(direction);
			human.desired_velocity = vec3(0.0f);
			if(glm::dot(human.getWeaponDirection(), direction / length) < std::cos(glm::radians(10.0f))) return true;
			for(PointComponentSlotId id : world.data.getChunk(chunk_id).point_component_slot_ids)
			{
				PointComponentSlot& component = world.data.getPointComponentSlot(id);
				if(component.point_id != point_id || component.type_id != PointComponentTypeId::inventory) continue;
				InventoryComponent& inventory = *static_cast<InventoryComponent*>(component.getData());
				for(InventorySlot& item : inventory.slots)
				{
					if(item.isEmpty() || !item.is_equipped || getItemDefinition(item.item_definition_id).type_id != item_type_ids::weapon) continue;
					useItem(world, point_id, item);
					return true;
				}
			}
			return true;
		}

}
