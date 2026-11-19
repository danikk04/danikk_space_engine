#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	/** @brief Resolves five supported, body-local positions for an allied or pirate column. */
	uint8 collectEventSpawnPlacements(
		World& world, const RaycastHit& hit, ToolActionId action, EventSpawnPlacement (&placements)[5])
	{
		if((action != ToolActionId::spawn_5_allies && action != ToolActionId::spawn_5_pirates)
			|| !hit.hit || !world.world_body_data.isWorldBodyActive(hit.body_id))
			return 0;
		const WorldBodyId body_id = hit.body_id;
		const WorldBody& body = world.world_body_data.getWorldBody(body_id);
		const glm::dvec3 body_position = body.position;
		const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
		if(glm::length(hit.normal) <= 1e-6f) return 0;
		const vec3 surface_up = glm::normalize(hit.normal);
		const PointComponentSlot& camera_slot = world.getCameraSlot();
		const WorldBody& camera_body = world.world_body_data.getWorldBody(
			world.chunks.getChunk(camera_slot.chunk_id).world_body_id);
		const vec3 camera_right = vec3(camera_body.rotation
			* glm::dvec3(world.getCameraComponent().getRight()));
		vec3 tangent = camera_right - surface_up * glm::dot(camera_right, surface_up);
		if(glm::length(tangent) <= 1e-6f)
		{
			const vec3 reference = glm::abs(surface_up.z) < 0.9f
				? vec3(0.0f, 0.0f, 1.0f) : vec3(0.0f, 1.0f, 0.0f);
			tangent = glm::cross(surface_up, reference);
		}
		tangent = glm::normalize(tangent);
		constexpr float column_spacing = 1.5f;
		uint8 placement_count = 0;
		for(uint8 index = 0; index < 5; ++index)
		{
			const glm::dvec3 origin = hit.position
				+ glm::dvec3(tangent * ((static_cast<float>(index) - 2.0f) * column_spacing)
					+ surface_up * 3.0f);
			const RaycastHit ground = world.raycast(origin, -surface_up, 6.0f);
			if(!ground.hit || ground.body_id != body_id || glm::dot(ground.normal, surface_up) < 0.5f) continue;
			const glm::dvec3 local_position = inverse_rotation * (ground.position - body_position);
			const vec3 local_surface_up = glm::normalize(vec3(inverse_rotation * glm::dvec3(ground.normal)));
			const glm::dvec3 gravity = world.environment.sampleGravityAt(ground.position);
			const glm::dvec3 world_up = glm::length(gravity) > 1e-8
				? -glm::normalize(gravity) : glm::normalize(glm::dvec3(ground.normal));
			const vec3 local_up = vec3(inverse_rotation * world_up);
			const vec3 forward = vec3(inverse_rotation * (world.getCameraWorldPosition() - ground.position));
			placements[placement_count++] = {
				ground.chunk_id, local_position + glm::dvec3(local_surface_up * 0.02f), local_up, forward};
		}
		return placement_count;
	}

	/** @brief Spawns five allied or pirate soldiers in the resolved column positions. */
	ToolApplyResult applyEventTool(World& world, const RaycastHit& hit, ToolActionId action)
	{
		EventSpawnPlacement placements[5];
		const uint8 placement_count = collectEventSpawnPlacements(world, hit, action, placements);
		const FactionId faction_id = action == ToolActionId::spawn_5_pirates
			? base_faction_ids::pirates : base_faction_ids::player;
		for(uint8 index = 0; index < placement_count; ++index)
		{
			const EventSpawnPlacement& placement = placements[index];
			createNpc(world, placement.chunk_id, placement.feet_position,
				placement.up, placement.forward, faction_id);
		}
		return {};
	}
}
