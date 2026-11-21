#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>

#include "terrain_tree_internal.h"

namespace danikk_space_engine::planet_terrain_tree_internal
{
	namespace
	{
		constexpr bool log_tree_diagnostics = true;
	}

	void logDiagnostics(const UpdateContext& context)
	{
		DSE_WATCH();
		if constexpr(log_tree_diagnostics)
		{
			static uint32 report_timestamp_ms = 0;
			const uint32 now = danikk_engine::getMsTimestamp();
			if(report_timestamp_ms == 0) report_timestamp_ms = now;
			if(now - report_timestamp_ms < 1000) return;

			size_t active_node_count = 0;
			double finest_active_half_extent = 0.0;
			for(const PlanetTerrainNode& node : context.tree.nodes)
			{
				if(!node.render_active) continue;
				active_node_count++;
				if(finest_active_half_extent == 0.0
					|| node.half_extent < finest_active_half_extent)
					finest_active_half_extent = node.half_extent;
			}
			size_t fine_request_count = 0;
			double finest_request_width = 0.0;
			for(const MeshGenerationRequest& request : context.planet.pending_requests)
			{
				if(request.node_width <= 128.0) fine_request_count++;
				if(finest_request_width == 0.0 || request.node_width < finest_request_width)
					finest_request_width = request.node_width;
			}
			danikk_framework::formatLogInfo(
				"Planet surface tree: body=% height=% detail_scale=% detail_adjustments=% target_nodes=% nodes=% request_candidates=% queued=% fine_queued=% finest_request_width=% active=% finest_active_half_extent=%",
				context.planet.world_body_id, context.height, context.detail_scale,
				context.detail_scale_adjustments, context.target_node_count,
				context.tree.nodes.size(), context.request_nodes.size(),
				context.planet.pending_requests.size(), fine_request_count,
				finest_request_width, active_node_count, finest_active_half_extent);
			report_timestamp_ms = now;
		}
	}
}
