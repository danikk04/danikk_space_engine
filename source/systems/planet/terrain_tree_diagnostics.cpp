#include <danikk_space_engine/systems/telemetry/telemetry.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	void recordDiagnostics(const UpdateContext& context)
	{
		DSE_WATCH();
		PlanetTerrainTreeTelemetry snapshot;
		snapshot.nodes = context.tree.nodes.size();
		snapshot.target_nodes = context.target_node_count;
		snapshot.detail_scale_adjustments = context.detail_scale_adjustments;
		snapshot.request_candidates = context.request_nodes.size();
		snapshot.queued_requests = context.planet.pending_requests.size();
		snapshot.detail_scale_total = context.detail_scale;
		for(const PlanetTerrainNode& node : context.tree.nodes)
		{
			if(node.render_active)
			{
				snapshot.active_nodes++;
				if(snapshot.finest_active_half_extent == 0.0
					|| node.half_extent < snapshot.finest_active_half_extent)
					snapshot.finest_active_half_extent = node.half_extent;
			}
		}
		for(const MeshGenerationJob& request : context.planet.pending_requests)
		{
			if(request.node_width <= 128.0) snapshot.fine_queued++;
			if(snapshot.finest_request_width == 0.0
				|| request.node_width < snapshot.finest_request_width)
				snapshot.finest_request_width = request.node_width;
		}
		telemetry_system.recordPlanetTerrainTree(snapshot);
	}
}
