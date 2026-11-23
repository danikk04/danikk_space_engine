#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/planet/system.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/environment/environment.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/human/system.h>

#include <danikk_space_engine/systems/raycast/raycast.h>

namespace danikk_space_engine
{
	/** @brief Resolves five supported, body-local positions for an allied or pirate column. */
	uint8 collectEventSpawnPlacements(
		World& world, const RaycastHit& hit, FactionId, EventSpawnPlacement (&placements)[5])
	{
		if(!hit.hit || !world.getWorldBodySystem().isWorldBodyActive(hit.body_id))
			return 0;
		const WorldBodyId body_id = hit.body_id;
		const WorldBody& body = world.getWorldBodySystem().getWorldBody(body_id);
		const glm::dvec3 body_position = body.position;
		const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
		if(glm::length(hit.normal) <= 1e-6f) return 0;
		const vec3 surface_up = glm::normalize(hit.normal);
		const vec3 camera_right = world.getCameraSystem().getRight();
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
			const glm::dvec3 gravity = world.getEnvironmentSystem().sampleGravityAt(spawn_world_position);
			const glm::dvec3 world_up = glm::length(gravity) > 1e-8
				? -glm::normalize(gravity) : glm::normalize(glm::dvec3(surface_up));
			const vec3 local_up = vec3(inverse_rotation * world_up);
			const vec3 forward = vec3(inverse_rotation * (world.getCameraSystem().getWorldPosition() - spawn_world_position));
			placements[index] = {body_id, local_position, local_up, forward};
		}
		return 5;
	}

	/** @brief Spawns five allied or pirate soldiers in the resolved column positions. */
	static ToolApplyResult spawnEventColumn(World& world, const RaycastHit& hit, FactionId faction_id)
	{
		EventSpawnPlacement placements[5];
		const uint8 placement_count = collectEventSpawnPlacements(world, hit, faction_id, placements);
		for(uint8 index = 0; index < placement_count; ++index)
		{
			EventSpawnPlacement& placement = placements[index];
			world.getHumanSystem().createNpc(placement.body_id, placement.feet_position,
				placement.up, placement.forward, faction_id);
		}
		return {};
	}

	ToolApplyResult applySpawnAlliesTool(World& world, const RaycastHit& hit, const vec3&, uint8)
	{
		return spawnEventColumn(world, hit, base_faction_ids::player);
	}

	ToolApplyResult applySpawnPiratesTool(World& world, const RaycastHit& hit, const vec3&, uint8)
	{
		return spawnEventColumn(world, hit, base_faction_ids::pirates);
	}
}
