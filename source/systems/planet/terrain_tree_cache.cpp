#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void evictCachedMeshes(UpdateContext& context)
	{
		DSE_WATCH();
		constexpr size_t maximum_cached_surface_meshes = 4096;
		size_t cached_mesh_count = 0;
		uint64 evicted_mesh_count = 0;
		for(const PlanetTerrainNode& node : context.tree.nodes)
			if(node.mesh.mesh != nullptr) cached_mesh_count++;

		while(cached_mesh_count > maximum_cached_surface_meshes)
		{
			PlanetTerrainNodeId eviction_id = invalid_node;
			double farthest_distance_squared = -1.0;
			for(PlanetTerrainNodeId node_id = 0; node_id < context.tree.nodes.size(); ++node_id)
			{
				const PlanetTerrainNode& node = context.tree.nodes[node_id];
				if(node.render_active || node.generation_pending || node.mesh.mesh == nullptr) continue;
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
			cached_mesh_count--;
			evicted_mesh_count++;
		}
		telemetry_system.recordPlanetMeshEviction(evicted_mesh_count);
		uint64 active_mesh_count = 0;
		for(const PlanetTerrainNode& node : context.tree.nodes)
			if(node.render_active && node.mesh.mesh != nullptr) active_mesh_count++;
		telemetry_system.recordPlanetTerrainCache(cached_mesh_count,
			active_mesh_count, maximum_cached_surface_meshes);
	}

	void destroyTree(PlanetData& planet)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : planet.terrain_tree.nodes)
			if(node.mesh.mesh != nullptr) destroyMeshRenderData(node.mesh);
		planet.terrain_tree.nodes.clear();
		planet.terrain_tree.root_id = invalid_node;
	}
}
