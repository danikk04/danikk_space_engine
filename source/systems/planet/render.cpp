#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
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

	/** @brief Draws active terrain-node meshes directly from their owning octree nodes. */
	void PlanetSystem::renderActiveTerrainNodes()
	{
		DSE_WATCH();
		const glm::dvec3 camera_position = world.getCameraSystem().getWorldPosition();
		constexpr uint terrain_depth_offset_location = 10;
		constexpr float maximum_terrain_depth_offset = 0.01f;
		constexpr float terrain_depth_offset_per_level = maximum_terrain_depth_offset
			/ static_cast<float>(maximum_terrain_ancestor_levels);

		// Draw no more than the camera's current target depth and its four coarser ancestors.
		// Fine meshes go first; each coarser level receives a larger clip-space depth bias.
		for(int depth = static_cast<int>(PlanetTerrainTree::maximum_depth); depth >= 0; --depth)
		{
			for(const PlanetData& planet : planets)
			{
				const uint8 current_depth = getCurrentTerrainDepth(planet);
				const uint8 coarsest_depth = current_depth > maximum_terrain_ancestor_levels
					? static_cast<uint8>(current_depth - maximum_terrain_ancestor_levels) : 0;
				if(depth < coarsest_depth || depth > current_depth) continue;
				const float depth_offset = static_cast<float>(current_depth - depth)
					* terrain_depth_offset_per_level;
				setFloatUniform(terrain_depth_offset_location, depth_offset);
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
		setFloatUniform(terrain_depth_offset_location, 0.0f);
	}
}
