#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/systems/planet/system.h>

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
		const vec3 camera_right = world.camera.getRight();
		vec3 tangent = camera_right - surface_up * glm::dot(camera_right, surface_up);
		if(glm::length(tangent) <= 1e-6f)
		{
			const vec3 reference = glm::abs(surface_up.z) < 0.9f
				? vec3(0.0f, 0.0f, 1.0f) : vec3(0.0f, 1.0f, 0.0f);
			tangent = glm::cross(surface_up, reference);
		}
		tangent = glm::normalize(tangent);
		constexpr float column_spacing = 1.5f;
		for(uint8 index = 0; index < 5; ++index)
		{
			const float tangent_offset = (static_cast<float>(index) - 2.0f) * column_spacing;
			const glm::dvec3 spawn_world_position = hit.position
				+ glm::dvec3(tangent * tangent_offset + surface_up * 0.02f);
			const glm::dvec3 local_position = inverse_rotation * (spawn_world_position - body_position);
			if(!world.chunks.isChunkActive(hit.chunk_id) && !world.planets.isPlanet(body_id)) return 0;
			const glm::dvec3 gravity = world.environment.sampleGravityAt(spawn_world_position);
			const glm::dvec3 world_up = glm::length(gravity) > 1e-8
				? -glm::normalize(gravity) : glm::normalize(glm::dvec3(surface_up));
			const vec3 local_up = vec3(inverse_rotation * world_up);
			const vec3 forward = vec3(inverse_rotation * (world.camera.getWorldPosition() - spawn_world_position));
			placements[index] = {hit.chunk_id, local_position, local_up, forward};
		}
		return 5;
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
			EventSpawnPlacement& placement = placements[index];
			if(!world.chunks.isChunkActive(placement.chunk_id) && world.planets.isPlanet(hit.body_id))
			{
				const glm::ivec3 coordinate(glm::floor(placement.feet_position / planet_chunk_width));
				placement.chunk_id = world.planets.getOrCreateSpatialChunk(hit.body_id, coordinate);
			}
			if(!world.chunks.isChunkActive(placement.chunk_id)) continue;
			createNpc(world, placement.chunk_id, placement.feet_position,
				placement.up, placement.forward, faction_id);
		}
		return {};
	}
}
