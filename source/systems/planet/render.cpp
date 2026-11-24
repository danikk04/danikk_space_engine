#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint8 maximum_terrain_ancestor_levels = 4;

		/** @brief Reproduces the mesh scheduler's current detail depth at its surface anchor. */
		uint8 getCurrentTerrainDepth(const PlanetData& planet)
		{
			glm::dvec3 anchor = planet.streaming_camera_position;
			if(planet.streaming_height >= 0.0)
			{
				const double camera_radius = glm::length(planet.streaming_camera_position);
				if(camera_radius > 1e-9)
					anchor *= (camera_radius - planet.streaming_height) / camera_radius;
			}

			const double target_half_extent = PlanetTerrainTree::getTargetHalfExtent(
				planet.streaming_camera_position, planet.streaming_detail_scale, anchor);
			uint8 depth = 0;
			double level_half_extent = PlanetTerrainTree::initial_root_half_extent;
			while(depth < PlanetTerrainTree::maximum_depth && level_half_extent > target_half_extent)
			{
				level_half_extent *= 0.5;
				depth++;
			}
			return depth;
		}
	}

	/** @brief Draws resident terrain meshes from fine nodes toward coarser ancestors. */
	void PlanetSystem::renderPlanetTerrainMeshes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = world.getCameraSystem().getWorldPosition();
		static_asset_collection.planet_terrain_shader.use();

		// Surface rendering includes up to four coarser ancestors; underground rendering
		// keeps only the current target depth so coarse surface meshes cannot cover caves.
		// Fine meshes are submitted before coarser ancestors.
		for(int depth = static_cast<int>(PlanetTerrainTree::maximum_depth); depth >= 0; --depth)
		{
			for(const PlanetData& planet : planets)
			{
				const uint8 current_depth = getCurrentTerrainDepth(planet);
				uint8 coarsest_depth = current_depth;
				if(planet.streaming_height >= 0.0)
					coarsest_depth = current_depth > maximum_terrain_ancestor_levels
						? static_cast<uint8>(current_depth - maximum_terrain_ancestor_levels) : 0;
				if(depth < coarsest_depth || depth > current_depth) continue;
				for(const PlanetTerrainNode& node : planet.terrain_tree.nodes)
				{
					if(node.depth != depth || node.mesh.mesh == nullptr) continue;
					const glm::dvec3 relative_position = planet.position
						+ planet.rotation * node.mesh_position - camera_position;
					mat4 body_rotation(1.0f);
					body_rotation[0] = vec4(vec3(planet.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
					body_rotation[1] = vec4(vec3(planet.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
					body_rotation[2] = vec4(vec3(planet.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
					const mat4 world_matrix = glm::translate(mat4(1.0f), vec3(relative_position)) * body_rotation;
					drawMeshRenderData(node.mesh, world_matrix);
				}
			}
		}
	}
}
