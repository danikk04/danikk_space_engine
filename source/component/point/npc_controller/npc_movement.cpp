#include <cmath>

#include <danikk_space_engine/world/world.h>
#include "internal.h"

namespace danikk_space_engine::npc_internal
{
	/** @brief Updates an NPC movement command and optional camera tracking. */
	void updateNpcMovement(World& world, PointComponentSlot& slot, NpcControllerComponent& controller, HumanComponent& human, float dt)
	{
		if(controller.has_movement_target)
		{
			vec3 offset = controller.target_position - world.data.getPoint(slot.point_id).position;
			offset -= human.up * glm::dot(offset, human.up);
			const float distance = glm::length(offset);
			if(distance <= 0.02f)
			{
				controller.has_movement_target = false;
			}
			else if(dt > 0.0f)
				human.desired_velocity = offset / distance
					* glm::min(glm::max(human.getMovementSpeed(), 0.0f), distance / dt);
		}

		if(controller.look_at_camera && world.data.isPointComponentSlotActive(world.camera_component_slot_id))
		{
			const Chunk& chunk = world.data.getChunk(slot.chunk_id);
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			const glm::dvec3 head_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id)
				+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.375f));
			const vec3 direction = vec3(glm::conjugate(body.rotation)
				* (world.getCameraWorldPosition() - head_position));
			human.lookAtDirection(direction);
		}
	}
}
