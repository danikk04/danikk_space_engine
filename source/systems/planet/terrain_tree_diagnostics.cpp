#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void PlanetSystem::recordDiagnostics(const PlanetData& planet,
		size_t target_node_count, size_t detail_scale_adjustments, double detail_scale)
	{
		DSE_WATCH();
		PlanetTerrainTreeTelemetry snapshot;
		snapshot.nodes = planet.terrain_tree.nodes.size();
		snapshot.target_nodes = target_node_count;
		snapshot.detail_scale_adjustments = detail_scale_adjustments;
		snapshot.detail_scale_total = detail_scale;
		for(const PlanetTerrainNode& node : planet.terrain_tree.nodes)
		{
			if(node.render_active)
			{
				snapshot.active_nodes++;
				if(snapshot.finest_active_half_extent == 0.0
					|| node.half_extent < snapshot.finest_active_half_extent)
					snapshot.finest_active_half_extent = node.half_extent;
			}
		}
		for(const PlanetTerrainNode& node : planet.terrain_tree.nodes)
		{
			if(!node.generation_pending) continue;
			snapshot.generation_pending_nodes++;
			snapshot.queued_jobs++;
			const double node_width = node.half_extent * 2.0;
			if(node_width <= 128.0) snapshot.fine_queued++;
			if(snapshot.finest_job_width == 0.0
				|| node_width < snapshot.finest_job_width)
				snapshot.finest_job_width = node_width;
		}
		world.getTelemetrySystem().mesh_generation_telemetry.recordPlanetTerrainTree(snapshot);
	}
}
