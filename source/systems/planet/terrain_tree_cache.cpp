#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	namespace
	{
		/** @brief Returns the octree depth implied by a node's power-of-two half extent. */
		uint8 getNodeDepth(const PlanetTerrainNode& node)
		{
			uint8 depth = 0;
			double half_extent = PlanetTerrainTree::initial_root_half_extent;
			while(depth < PlanetTerrainTree::maximum_depth && half_extent > node.half_extent)
			{
				half_extent *= 0.5;
				depth++;
			}
			return depth;
		}
	}

	void evictCachedMeshes(UpdateContext& context)
	{
		DSE_WATCH();
		constexpr size_t maximum_cached_meshes_per_level = 1024;
		size_t cached_meshes_per_level[PlanetTerrainTree::maximum_depth + 1] = {};
		size_t cached_mesh_count = 0;
		uint64 evicted_mesh_count = 0;
		for(const PlanetTerrainNode& node : context.tree.nodes)
		{
			if(node.mesh.mesh == nullptr) continue;
			cached_meshes_per_level[getNodeDepth(node)]++;
			cached_mesh_count++;
		}

		for(uint8 level = 0; level <= PlanetTerrainTree::maximum_depth; ++level)
		{
			double level_half_extent = PlanetTerrainTree::initial_root_half_extent;
			for(uint8 depth = 0; depth < level; ++depth) level_half_extent *= 0.5;
			while(cached_meshes_per_level[level] > maximum_cached_meshes_per_level)
			{
				PlanetTerrainNodeId eviction_id = invalid_node;
				double farthest_distance_squared = -1.0;
				for(PlanetTerrainNodeId node_id = 0; node_id < context.tree.nodes.size(); ++node_id)
				{
					const PlanetTerrainNode& node = context.tree.nodes[node_id];
					if(node.half_extent != level_half_extent || node.render_active || node.generation_pending
						|| node.mesh.mesh == nullptr) continue;
					const glm::dvec3 offset = node.center - context.camera_position;
					const double distance_squared = glm::dot(offset, offset);
					if(distance_squared <= farthest_distance_squared) continue;
					farthest_distance_squared = distance_squared;
					eviction_id = node_id;
				}
				if(eviction_id == invalid_node) break;
				PlanetTerrainNode& node = context.tree.nodes[eviction_id];
				destroyMeshRenderData(node.mesh);
				node.mesh_revision = 0;
				cached_meshes_per_level[level]--;
				cached_mesh_count--;
				evicted_mesh_count++;
			}
		}
		context.telemetry.recordPlanetMeshEviction(evicted_mesh_count);
		uint64 active_mesh_count = 0;
		for(const PlanetTerrainNode& node : context.tree.nodes)
			if(node.render_active && node.mesh.mesh != nullptr) active_mesh_count++;
		context.telemetry.recordPlanetTerrainCache(cached_mesh_count,
			active_mesh_count,
			(maximum_cached_meshes_per_level * (PlanetTerrainTree::maximum_depth + 1)));
	}

	void destroyTree(PlanetData& planet)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : planet.terrain_tree.nodes)
			if(node.mesh.mesh != nullptr) destroyMeshRenderData(node.mesh);
		planet.terrain_tree.clear();
	}
}
