#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/** @brief Draws active planet surface meshes directly from their owning octree nodes. */
	void PlanetSystem::renderSurfaceNodes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = world.camera.getWorldPosition();
		for(const PlanetData& planet : planets)
		{
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			for(const PlanetTerrainNode& node : planet.terrain_tree.nodes)
			{
				if(!node.render_active || node.mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = body.position
					+ body.rotation * node.mesh_position - camera_position;
				mat4 body_rotation(1.0f);
				body_rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				body_rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				body_rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				const mat4 world_matrix = glm::translate(mat4(1.0f), vec3(relative_position)) * body_rotation;
				drawMeshRenderData(node.mesh, world_matrix);
			}
		}
	}
}
