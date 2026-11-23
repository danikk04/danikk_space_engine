#include <danikk_space_engine/effects/event_spawn_preview.h>

#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/** @brief Draws five blue human silhouettes using the exact placements used by the event. */
	void drawEventSpawnPreview(World& world, const RaycastHit& hit, FactionId faction_id)
	{
		EventSpawnPlacement placements[5];
		const uint8 placement_count = collectEventSpawnPlacements(world, hit, faction_id, placements);
		if(!world.world_body_data.isWorldBodyActive(hit.body_id)) return;
		const WorldBody& body = world.world_body_data.getWorldBody(hit.body_id);
		const vec4 preview_color(0.08f, 0.35f, 1.0f, 0.55f);
		for(uint8 index = 0; index < placement_count; ++index)
		{
			const EventSpawnPlacement& placement = placements[index];
			const vec3 local_up = glm::normalize(placement.up);
			vec3 local_forward = placement.forward - local_up * glm::dot(placement.forward, local_up);
			if(glm::length(local_forward) <= 1e-6f)
			{
				const vec3 reference = glm::abs(local_up.z) < 0.9f
					? vec3(0.0f, 0.0f, 1.0f) : vec3(0.0f, 1.0f, 0.0f);
				local_forward = reference - local_up * glm::dot(reference, local_up);
			}
			local_forward = glm::normalize(local_forward);
			const vec3 local_right = glm::normalize(glm::cross(local_forward, local_up));
			local_forward = glm::normalize(glm::cross(local_up, local_right));
			const vec3 up = vec3(body.rotation * glm::dvec3(local_up));
			const vec3 forward = vec3(body.rotation * glm::dvec3(local_forward));
			const vec3 right = vec3(body.rotation * glm::dvec3(local_right));
			mat4 orientation(1.0f);
			orientation[0] = vec4(right, 0.0f);
			orientation[1] = vec4(forward, 0.0f);
			orientation[2] = vec4(up, 0.0f);
			HumanState preview_human;
			preview_human.up = local_up;
			preview_human.setInitialForward(local_forward);
			const float height = preview_human.getHeight();
			const glm::dvec3 center_world = body.position + body.rotation
				* (placement.feet_position + glm::dvec3(local_up * (height * 0.5f)));
			const mat4 body_matrix = glm::translate(mat4(1.0f),
				vec3(center_world - world.camera.getWorldPosition())) * orientation;
			const mat4 feet_matrix = body_matrix
				* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
				* glm::scale(mat4(1.0f), vec3(height));
			drawHumanAppearance(preview_human, feet_matrix, preview_color);
		}
	}
}
