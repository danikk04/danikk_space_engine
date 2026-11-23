#include <algorithm>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::evictCachedMeshes(PlanetTerrainTree& tree,
		const glm::dvec3& camera_position)
	{
		DSE_WATCH();
		constexpr size_t maximum_cached_meshes_per_level = 1024;
		size_t cached_meshes_per_level[PlanetTerrainTree::maximum_depth + 1] = {};
		size_t cached_mesh_count = 0;
		uint64 active_mesh_count = 0;
		uint64 evicted_mesh_count = 0;
		for(const PlanetTerrainNode& node : tree.nodes)
		{
			if(node.render_active && node.mesh.mesh != nullptr) active_mesh_count++;
			if(node.mesh.mesh == nullptr) continue;
			cached_meshes_per_level[node.depth]++;
			cached_mesh_count++;
		}

		bool cache_over_capacity = false;
		for(uint8 level = 0; level <= PlanetTerrainTree::maximum_depth; ++level)
			if(cached_meshes_per_level[level] > maximum_cached_meshes_per_level)
				cache_over_capacity = true;

		if(cache_over_capacity)
		{
			mesh_eviction_candidates.clear();
			for(PlanetTerrainNodeId node_id = 0; node_id < tree.nodes.size(); ++node_id)
			{
				const PlanetTerrainNode& node = tree.nodes[node_id];
				if(node.mesh.mesh == nullptr || node.render_active || node.generation_pending) continue;
				mesh_eviction_candidates.push(node_id);
			}
			std::sort(mesh_eviction_candidates.begin(), mesh_eviction_candidates.end(),
				[&tree, &camera_position](PlanetTerrainNodeId left_id, PlanetTerrainNodeId right_id)
				{
					const PlanetTerrainNode& left = tree.nodes[left_id];
					const PlanetTerrainNode& right = tree.nodes[right_id];
					if(left.depth != right.depth) return left.depth < right.depth;
					const glm::dvec3 left_offset = left.center - camera_position;
					const glm::dvec3 right_offset = right.center - camera_position;
					return glm::dot(left_offset, left_offset) > glm::dot(right_offset, right_offset);
				});

			for(PlanetTerrainNodeId node_id : mesh_eviction_candidates)
			{
				PlanetTerrainNode& node = tree.nodes[node_id];
				if(cached_meshes_per_level[node.depth] <= maximum_cached_meshes_per_level)
					continue;
				destroyMeshRenderData(node.mesh);
				world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
					MeshDeletionReason::cache_capacity);
				node.mesh_revision = 0;
				node.mesh_generated = false;
				cached_meshes_per_level[node.depth]--;
				cached_mesh_count--;
				evicted_mesh_count++;
			}
		}
		world.getTelemetrySystem().mesh_generation_telemetry.recordPlanetMeshEviction(evicted_mesh_count);
		world.getTelemetrySystem().mesh_generation_telemetry.recordPlanetTerrainCache(cached_mesh_count,
			active_mesh_count,
			(maximum_cached_meshes_per_level * (PlanetTerrainTree::maximum_depth + 1)));
	}
}
