#include <danikk_framework/log.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr bool enable_mesh_generation_telemetry_logs = false;
	}

	void MeshGenerationTelemetry::beginFrame()
	{
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_frame = PlanetTerrainTreeTelemetry();
	}

	void MeshGenerationTelemetry::endFrame()
	{
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_snapshot = terrain_tree_frame;
	}

	void MeshGenerationTelemetry::recordPlanetMeshGeneration(uint8 level, uint64 triangle_count,
		uint64 elapsed_nanoseconds)
	{
		std::lock_guard<std::mutex> guard(mutex);
		while(planet_mesh_levels.size() <= level) planet_mesh_levels.pushCtor();
		PlanetMeshLevelTelemetry& stats = planet_mesh_levels[level];
		stats.generated_meshes++;
		stats.generated_triangles += triangle_count;
		TelemetryTimingStats& timing = stats.generation_time;
		if(timing.samples == 0 || elapsed_nanoseconds < timing.minimum_nanoseconds)
			timing.minimum_nanoseconds = elapsed_nanoseconds;
		if(timing.samples == 0 || elapsed_nanoseconds > timing.maximum_nanoseconds)
			timing.maximum_nanoseconds = elapsed_nanoseconds;
		timing.total_nanoseconds += elapsed_nanoseconds;
		timing.samples++;
		has_updates = true;
	}

	void MeshGenerationTelemetry::recordPlanetMeshAcceptance(PlanetMeshAcceptance outcome)
	{
		std::lock_guard<std::mutex> guard(mutex);
		switch(outcome)
		{
			case PlanetMeshAcceptance::accepted: accepted_planet_meshes++; break;
			case PlanetMeshAcceptance::empty: empty_planet_meshes++; break;
			case PlanetMeshAcceptance::missing_planet: missing_planet_meshes++; break;
			case PlanetMeshAcceptance::stale_planet: stale_planet_meshes++; break;
			case PlanetMeshAcceptance::stale_terrain: stale_terrain_meshes++; break;
			case PlanetMeshAcceptance::invalid_node: invalid_planet_mesh_nodes++; break;
			case PlanetMeshAcceptance::stale_generator: stale_planet_mesh_generators++; break;
			case PlanetMeshAcceptance::stale_node: stale_planet_mesh_nodes++; break;
		}
		has_updates = true;
	}

	void MeshGenerationTelemetry::recordPlanetMeshEviction(uint64 count)
	{
		if(count == 0) return;
		std::lock_guard<std::mutex> guard(mutex);
		evicted_planet_meshes += count;
		has_updates = true;
	}

	void MeshGenerationTelemetry::recordPlanetTerrainCache(uint64 cached_meshes,
		uint64 active_meshes, uint64 cache_capacity)
	{
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_frame.cached_meshes += cached_meshes;
		terrain_tree_frame.active_meshes += active_meshes;
		terrain_tree_frame.cache_capacity += cache_capacity;
		has_updates = true;
	}

	void MeshGenerationTelemetry::setPlanetMeshQueueSnapshot(const PlanetMeshQueueTelemetry& snapshot)
	{
		std::lock_guard<std::mutex> guard(mutex);
		mesh_queue_snapshot = snapshot;
		has_updates = true;
	}

	void MeshGenerationTelemetry::recordPlanetTerrainTree(const PlanetTerrainTreeTelemetry& snapshot)
	{
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_frame.planets++;
		terrain_tree_frame.nodes += snapshot.nodes;
		terrain_tree_frame.active_nodes += snapshot.active_nodes;
		terrain_tree_frame.target_nodes += snapshot.target_nodes;
		terrain_tree_frame.detail_scale_adjustments += snapshot.detail_scale_adjustments;
		terrain_tree_frame.job_candidates += snapshot.job_candidates;
		terrain_tree_frame.queued_jobs += snapshot.queued_jobs;
		terrain_tree_frame.fine_queued += snapshot.fine_queued;
		if(snapshot.finest_job_width > 0.0
			&& (terrain_tree_frame.finest_job_width == 0.0
				|| snapshot.finest_job_width < terrain_tree_frame.finest_job_width))
			terrain_tree_frame.finest_job_width = snapshot.finest_job_width;
		if(snapshot.finest_active_half_extent > 0.0
			&& (terrain_tree_frame.finest_active_half_extent == 0.0
				|| snapshot.finest_active_half_extent < terrain_tree_frame.finest_active_half_extent))
			terrain_tree_frame.finest_active_half_extent = snapshot.finest_active_half_extent;
		terrain_tree_frame.detail_scale_total += snapshot.detail_scale_total;
		if(snapshot.cache_capacity > 0)
		{
			terrain_tree_frame.cached_meshes += snapshot.cached_meshes;
			terrain_tree_frame.active_meshes += snapshot.active_meshes;
			terrain_tree_frame.cache_capacity += snapshot.cache_capacity;
		}
		has_updates = true;
	}

	void MeshGenerationTelemetry::report()
	{
		if constexpr(!enable_mesh_generation_telemetry_logs) return;

		danikk_framework::DynamicArray<PlanetMeshLevelTelemetry> report_mesh_levels;
		uint64 report_accepted = 0;
		uint64 report_empty = 0;
		uint64 report_missing_planet = 0;
		uint64 report_stale_planet = 0;
		uint64 report_stale_terrain = 0;
		uint64 report_invalid_node = 0;
		uint64 report_stale_generator = 0;
		uint64 report_stale_node = 0;
		uint64 report_evicted = 0;
		PlanetMeshQueueTelemetry report_queue;
		PlanetTerrainTreeTelemetry report_tree;
		{
			std::lock_guard<std::mutex> guard(mutex);
			if(!has_updates) return;
			report_mesh_levels = planet_mesh_levels;
			report_accepted = accepted_planet_meshes;
			report_empty = empty_planet_meshes;
			report_missing_planet = missing_planet_meshes;
			report_stale_planet = stale_planet_meshes;
			report_stale_terrain = stale_terrain_meshes;
			report_invalid_node = invalid_planet_mesh_nodes;
			report_stale_generator = stale_planet_mesh_generators;
			report_stale_node = stale_planet_mesh_nodes;
			report_evicted = evicted_planet_meshes;
			report_queue = mesh_queue_snapshot;
			report_tree = terrain_tree_snapshot;

			planet_mesh_levels.clear();
			accepted_planet_meshes = 0;
			empty_planet_meshes = 0;
			missing_planet_meshes = 0;
			stale_planet_meshes = 0;
			stale_terrain_meshes = 0;
			invalid_planet_mesh_nodes = 0;
			stale_planet_mesh_generators = 0;
			stale_planet_mesh_nodes = 0;
			evicted_planet_meshes = 0;
			has_updates = false;
		}

		using danikk_framework::formatLogInfo;
		uint64 generated_meshes = 0;
		uint64 generated_triangles = 0;
		uint64 generation_nanoseconds = 0;
		for(const PlanetMeshLevelTelemetry& level : report_mesh_levels)
		{
			generated_meshes += level.generated_meshes;
			generated_triangles += level.generated_triangles;
			generation_nanoseconds += level.generation_time.total_nanoseconds;
		}
		const uint64 stale_jobs = report_missing_planet + report_stale_planet
			+ report_stale_terrain + report_invalid_node + report_stale_generator + report_stale_node;
		const double average_generation_milliseconds = generated_meshes == 0 ? 0.0
			: static_cast<double>(generation_nanoseconds) / generated_meshes / 1000000.0;
		const double average_detail_scale = report_tree.planets == 0 ? 0.0
			: report_tree.detail_scale_total / static_cast<double>(report_tree.planets);
		formatLogInfo(
			"Telemetry mesh: generated=% triangles=% avg_ms=% accepted=% empty=% stale=% evicted=% cache=%/% active=%",
			generated_meshes, generated_triangles, average_generation_milliseconds,
			report_accepted, report_empty, stale_jobs, report_evicted,
			report_tree.cached_meshes, report_tree.cache_capacity,
			report_tree.active_meshes);
		formatLogInfo(
			"Telemetry stream: queue=%/% fine=%/% active=%/% orphan_active=% completed=% planets=% tree=%/% target=% candidates=% pending=% fine_pending=% finest_job=% finest_active=% detail_scale=% scale_adjustments=%",
			report_queue.queued, report_queue.desired,
			report_queue.fine_queued, report_queue.fine_desired,
			report_queue.active, report_queue.fine_active,
			report_queue.active_not_desired, report_queue.completed, report_tree.planets,
			report_tree.nodes, report_tree.active_nodes, report_tree.target_nodes,
			report_tree.job_candidates, report_tree.queued_jobs,
			report_tree.fine_queued, report_tree.finest_job_width,
			report_tree.finest_active_half_extent, average_detail_scale,
			report_tree.detail_scale_adjustments);
		for(size_t level_id = 0; level_id < report_mesh_levels.size(); ++level_id)
		{
			const PlanetMeshLevelTelemetry& level = report_mesh_levels[level_id];
			if(level.generated_meshes == 0) continue;
			const double nanoseconds_per_millisecond = 1000000.0;
			formatLogInfo(
				"Planet mesh level=% generated=% triangles=% avg_ms=% min_ms=% max_ms=%",
				level_id, level.generated_meshes, level.generated_triangles,
				static_cast<double>(level.generation_time.total_nanoseconds)
					/ level.generation_time.samples / nanoseconds_per_millisecond,
				static_cast<double>(level.generation_time.minimum_nanoseconds) / nanoseconds_per_millisecond,
				static_cast<double>(level.generation_time.maximum_nanoseconds) / nanoseconds_per_millisecond);
		}
	}

	void TelemetrySystem::recordPlanetMeshGeneration(uint8 level, uint64 triangle_count,
		uint64 elapsed_nanoseconds)
	{
		mesh_generation_telemetry.recordPlanetMeshGeneration(level, triangle_count, elapsed_nanoseconds);
	}

	void TelemetrySystem::recordPlanetMeshAcceptance(PlanetMeshAcceptance outcome)
	{
		mesh_generation_telemetry.recordPlanetMeshAcceptance(outcome);
	}

	void TelemetrySystem::recordPlanetMeshEviction(uint64 count)
	{
		mesh_generation_telemetry.recordPlanetMeshEviction(count);
	}

	void TelemetrySystem::recordPlanetTerrainCache(uint64 cached_meshes,
		uint64 active_meshes, uint64 cache_capacity)
	{
		mesh_generation_telemetry.recordPlanetTerrainCache(cached_meshes, active_meshes, cache_capacity);
	}

	void TelemetrySystem::setPlanetMeshQueueSnapshot(const PlanetMeshQueueTelemetry& snapshot)
	{
		mesh_generation_telemetry.setPlanetMeshQueueSnapshot(snapshot);
	}

	void TelemetrySystem::recordPlanetTerrainTree(const PlanetTerrainTreeTelemetry& snapshot)
	{
		mesh_generation_telemetry.recordPlanetTerrainTree(snapshot);
	}

	void TelemetrySystem::reportMeshGenerationTelemetry()
	{
		mesh_generation_telemetry.report();
	}
}
