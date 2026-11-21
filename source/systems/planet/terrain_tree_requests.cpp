#include <algorithm>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void collectMeshRequests(UpdateContext& context)
	{
		DSE_WATCH();
		for(PlanetTerrainNode& node : context.tree.nodes) node.render_active = false;
		context.planet.pending_requests.clear();
		for(PlanetTerrainNode& node : context.tree.nodes) node.generation_pending = false;

		const auto collect = [&](auto&& self, PlanetTerrainNodeId node_id) -> void
		{
			const PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(!node.hasChildren()
				|| node.half_extent <= getTargetHalfExtent(context, node.center)
				|| !hasCurrentMesh(context, node_id))
			{
				if(!hasCurrentMesh(context, node_id)) context.request_nodes.push(node_id);
				return;
			}
			for(uint8 child_index = 0; child_index < 8; ++child_index)
			{
				const PlanetTerrainNodeId child_id = node.children[child_index];
				if(child_id != invalid_node && isNodeRelevant(context, context.tree.nodes[child_id]))
					self(self, child_id);
			}
		};
		collect(collect, context.tree.root_id);
		std::sort(context.request_nodes.begin(), context.request_nodes.end(),
			[&context](PlanetTerrainNodeId left_id, PlanetTerrainNodeId right_id)
			{
				const glm::dvec3 left_offset = context.tree.nodes[left_id].center - context.camera_position;
				const glm::dvec3 right_offset = context.tree.nodes[right_id].center - context.camera_position;
				return glm::dot(left_offset, left_offset) < glm::dot(right_offset, right_offset);
			});

		for(PlanetTerrainNodeId node_id : context.request_nodes)
		{
			if(context.planet.pending_requests.size() >= maximum_requests_per_frame) break;
			PlanetTerrainNode& node = context.tree.nodes[node_id];
			if(hasCurrentMesh(context, node_id)) continue;
			MeshGenerationRequest request;
			request.source = MeshGenerationSource::planet_terrain_node;
			request.world_body_id = context.planet.world_body_id;
			request.planet_generation_id = context.planet.generation_id;
			request.terrain_revision = context.planet.terrain_revision;
			request.terrain_node_id = node_id;
			request.terrain_node_generation_id = node.generation_id;
			request.surface_only = isSurfaceNode(context, node);
			request.planet_mesh_generator = settings.planet_mesh_generator;
			request.node_width = node.half_extent * 2.0;
			request.voxel_size = glm::max(2.0, request.node_width / 16.0);
			request.node_minimum = node.center - glm::dvec3(node.half_extent);
			context.planet.pending_requests.pushMove(request);
			node.generation_pending = true;
		}
	}
}
