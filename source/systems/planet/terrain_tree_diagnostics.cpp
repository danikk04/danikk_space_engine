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
		snapshot.job_candidates = context.mesh_job_candidate_node_ids.size();
		snapshot.queued_jobs = context.planet.pending_job_candidates.size();
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
		for(const MeshGenerationJob& candidate : context.planet.pending_job_candidates)
		{
			if(candidate.node_width <= 128.0) snapshot.fine_queued++;
			if(snapshot.finest_job_width == 0.0
				|| candidate.node_width < snapshot.finest_job_width)
				snapshot.finest_job_width = candidate.node_width;
		}
		context.telemetry.mesh_generation_telemetry.recordPlanetTerrainTree(snapshot);
	}
}
