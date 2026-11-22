#include <algorithm>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	size_t PlanetSystem::collectMeshJobCandidates(PlanetData& planet,
		const glm::dvec3& camera_position, double height, const PlanetSettings& planet_settings,
		double detail_scale, const glm::dvec3& render_window_minimum,
		const glm::dvec3& render_window_maximum)
	{
		DSE_WATCH();
		PlanetTerrainTree& tree = planet.terrain_tree;
		for(PlanetTerrainNode& node : tree.nodes) node.render_active = false;
		planet.pending_job_candidates.clear();
		for(PlanetTerrainNode& node : tree.nodes) node.generation_pending = false;
		DynamicArray<PlanetTerrainNodeId> candidate_node_ids;

		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = tree.nodes[node_id];
			if(node.half_extent > getTargetHalfExtent(camera_position, detail_scale, node.center))
			{
				if(!node.hasChildren()) return;
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != PlanetTerrainTree::invalid_node && isNodeRelevant(render_window_minimum,
						render_window_maximum, camera_position, height, planet_settings,
						tree.nodes[child_id]))
						self(self, child_id);
				}
				return;
			}
			const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius,
				25000.0, 15000.0);
			if(!node.hasCurrentMesh(planet.terrain_revision,
				::danikk_space_engine::settings.planet_mesh_generator, surface_only))
				candidate_node_ids.push(node_id);
		};
		if(tree.root_id >= tree.nodes.size()) return 0;
		collect(collect, tree.root_id);
		std::sort(candidate_node_ids.begin(), candidate_node_ids.end(),
			[&tree, &camera_position](PlanetTerrainNodeId left_id, PlanetTerrainNodeId right_id)
			{
				const glm::dvec3 left_offset = tree.nodes[left_id].center - camera_position;
				const glm::dvec3 right_offset = tree.nodes[right_id].center - camera_position;
				return glm::dot(left_offset, left_offset) < glm::dot(right_offset, right_offset);
			});

		for(PlanetTerrainNodeId node_id : candidate_node_ids)
		{
			if(planet.pending_job_candidates.size() >= maximum_job_candidates_per_frame) break;
			PlanetTerrainNode& node = tree.nodes[node_id];
			const bool surface_only = node.intersectsSurfaceBand(planet_settings.radius,
				25000.0, 15000.0);
			if(node.hasCurrentMesh(planet.terrain_revision,
				::danikk_space_engine::settings.planet_mesh_generator, surface_only)) continue;
			MeshGenerationJob candidate;
			candidate.source = MeshGenerationSource::planet_terrain_node;
			candidate.planet_id = planet.id;
			candidate.planet_generation_id = planet.generation_id;
			candidate.terrain_revision = planet.terrain_revision;
			candidate.terrain_node_id = node_id;
			candidate.terrain_node_generation_id = node.generation_id;
			candidate.surface_only = surface_only;
			candidate.planet_mesh_generator = ::danikk_space_engine::settings.planet_mesh_generator;
			candidate.node_width = node.half_extent * 2.0;
			candidate.voxel_size = glm::max(2.0, candidate.node_width / 16.0);
			candidate.node_minimum = node.center - glm::dvec3(node.half_extent);
			planet.pending_job_candidates.pushMove(candidate);
			node.generation_pending = true;
		}
		return candidate_node_ids.size();
	}
}
