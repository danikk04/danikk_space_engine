#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	/** @brief Spawns up to five pirate soldiers on nearby supported surface points. */
	ToolApplyResult applyEventTool(World& world, const RaycastHit& hit, ToolActionId action)
	{
		if(action != ToolActionId::raid || !hit.hit || !world.data.isWorldBodyActive(hit.body_id))
			return {};
		const WorldBodyId body_id = hit.body_id;
		const glm::dvec3 body_position = world.data.getWorldBody(body_id).position;
		const glm::dquat inverse_rotation = glm::conjugate(world.data.getWorldBody(body_id).rotation);
		const vec3 surface_up = glm::normalize(hit.normal);
		const vec3 reference = glm::abs(surface_up.z) < 0.9f ? vec3(0, 0, 1) : vec3(0, 1, 0);
		const vec3 tangent = glm::normalize(glm::cross(surface_up, reference));
		const vec3 bitangent = glm::cross(surface_up, tangent);
		const vec2 offsets[] = {{0, 0}, {2, 0}, {-2, 0}, {0, 2}, {0, -2}};
		for(const vec2& offset : offsets)
		{
			const glm::dvec3 origin = hit.position + glm::dvec3(tangent * offset.x + bitangent * offset.y + surface_up * 3.0f);
			const RaycastHit ground = world.raycast(origin, -surface_up, 6.0f);
			if(!ground.hit || ground.body_id != body_id || glm::dot(ground.normal, surface_up) < 0.5f) continue;
			const glm::dvec3 local_position = inverse_rotation * (ground.position - body_position);
			const vec3 local_up = glm::normalize(vec3(inverse_rotation * glm::dvec3(ground.normal)));
			const vec3 forward = vec3(inverse_rotation * (world.getCameraWorldPosition() - ground.position));
			const PointComponentSlotId controller_id = createNpc(world, body_id,
				local_position + glm::dvec3(local_up * 0.02f), local_up, forward);
			const NpcControllerComponent& controller = *static_cast<const NpcControllerComponent*>(
				world.data.getPointComponentSlot(controller_id).getData());
			HumanComponent& human = *static_cast<HumanComponent*>(
				world.data.getPointComponentSlot(controller.human_slot_id).getData());
			human.faction_id = base_faction_ids::pirates;
		}
		return {};
	}
}
