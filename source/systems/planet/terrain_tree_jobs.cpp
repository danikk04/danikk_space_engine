#include <algorithm>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void collectMeshJobCandidates(UpdateContext& context)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : context.tree.nodes) node.render_active = false;
		context.planet.pending_job_candidates.clear();
		for(PlanetTerrainNode& node : context.tree.nodes) node.generation_pending = false;

		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(node.half_extent > getTargetHalfExtent(context, node.center))
			{
				if(!node.hasChildren()) return;
				for(uint8 child_index = 0; child_index < 8; ++child_index)
				{
					const PlanetTerrainNodeId child_id = node.children[child_index];
					if(child_id != invalid_node && isNodeRelevant(context, context.tree.nodes[child_id]))
						self(self, child_id);
				}
				return;
			}
			if(!hasCurrentMesh(context, node_id)) context.mesh_job_candidate_node_ids.push(node_id);
		};
		collect(collect, context.tree.root_id);
		std::sort(context.mesh_job_candidate_node_ids.begin(), context.mesh_job_candidate_node_ids.end(),
			[&context](PlanetTerrainNodeId left_id, PlanetTerrainNodeId right_id)
			{
				const glm::dvec3 left_offset = context.tree.nodes[left_id].center - context.camera_position;
				const glm::dvec3 right_offset = context.tree.nodes[right_id].center - context.camera_position;
				return glm::dot(left_offset, left_offset) < glm::dot(right_offset, right_offset);
			});

		for(PlanetTerrainNodeId node_id : context.mesh_job_candidate_node_ids)
		{
			if(context.planet.pending_job_candidates.size() >= maximum_job_candidates_per_frame) break;
			PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(hasCurrentMesh(context, node_id)) continue;
			MeshGenerationJob candidate;
			candidate.source = MeshGenerationSource::planet_terrain_node;
			candidate.planet_id = context.planet.id;
			candidate.planet_generation_id = context.planet.generation_id;
			candidate.terrain_revision = context.planet.terrain_revision;
			candidate.terrain_node_id = node_id;
			candidate.terrain_node_generation_id = node.generation_id;
			candidate.surface_only = isSurfaceNode(context, node);
			candidate.planet_mesh_generator = settings.planet_mesh_generator;
			candidate.node_width = node.half_extent * 2.0;
			candidate.voxel_size = glm::max(2.0, candidate.node_width / 16.0);
			candidate.node_minimum = node.center - glm::dvec3(node.half_extent);
			context.planet.pending_job_candidates.pushMove(candidate);
			node.generation_pending = true;
		}
	}
}
