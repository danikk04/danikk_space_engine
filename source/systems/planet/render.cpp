#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	/** @brief Draws the terrain meshes and transforms collected by each planet for this frame. */
	void PlanetSystem::renderPlanetTerrainMeshes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = world.getCameraSystem().getWorldPosition();
		static_asset_collection.planet_terrain_shader.use();

		for(const PlanetData& planet : planets)
		{
			for(const PlanetTerrainRenderItem& item : planet.terrain_render_items)
			{
				if(item.node_id >= planet.terrain_tree.nodes.size()) continue;
				const MeshRenderData& mesh = planet.terrain_tree.nodes[item.node_id].mesh;
				if(mesh.mesh == nullptr) continue;
				const glm::dvec3 relative_position = item.world_position - camera_position;
				mat4 body_rotation(1.0f);
				body_rotation[0] = vec4(vec3(item.world_rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
				body_rotation[1] = vec4(vec3(item.world_rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
				body_rotation[2] = vec4(vec3(item.world_rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
				const mat4 world_matrix = glm::translate(mat4(1.0f), vec3(relative_position)) * body_rotation;
				drawMeshRenderData(mesh, world_matrix);
			}
		}
	}
}
