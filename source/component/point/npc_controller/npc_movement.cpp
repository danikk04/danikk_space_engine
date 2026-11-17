#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include "internal.h"

namespace danikk_space_engine::npc_internal
{
	/** @brief Executes the movement and look targets selected centrally by UnitSystem. */
	void executeNpcMovementOrder(World& world, PointComponentSlot& slot,
		NpcControllerComponent& controller, HumanComponent& human, float dt)
	{
		DSE_WATCH();
		const Chunk& chunk = world.data.getChunk(slot.chunk_id);
		const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		const glm::dvec3 own_position = world.getPointWorldPosition(slot.chunk_id, slot.point_id);
		if(controller.order.has_move_target)
		{
			const glm::dvec3 body_local_target = glm::conjugate(body.rotation)
				* (controller.order.move_target_world - body.position);
			const vec3 target_in_chunk = vec3(body_local_target - chunk.position);
			vec3 offset = target_in_chunk - world.data.getPoint(slot.point_id).position;
			offset -= human.up * glm::dot(offset, human.up);
			const float distance = glm::length(offset);
			if(distance > 0.02f && dt > 0.0f)
				human.desired_velocity = offset / distance
					* glm::min(glm::max(human.getMovementSpeed(), 0.0f), distance / dt);
		}

		if(controller.order.has_look_target)
		{
			const glm::dvec3 head_position = own_position
				+ body.rotation * glm::dvec3(human.up * (human.getHeight() * 0.375f));
			const vec3 direction = vec3(glm::conjugate(body.rotation)
				* (controller.order.look_target_world - head_position));
			human.lookAtDirection(direction);
		}
	}
}
