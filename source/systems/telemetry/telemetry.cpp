#include <cstring>

#include <danikk_framework/log.h>

#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	namespace
	{
		constexpr uint32 invalid_node_id = static_cast<uint32>(-1);
		thread_local DynamicArray<uint32> scope_stack;

		/**
		 * @brief Caches a profiler scope name and parent/node IDs for repeated scope lookup.
		 */
		struct ScopeCacheEntry
		{
			const char* name = nullptr;
			uint32 parent_id = invalid_node_id;
			uint32 node_id = invalid_node_id;
		};
		thread_local danikk_framework::DynamicArray<ScopeCacheEntry> scope_cache;
	}

	/**
	 * @brief Initializes the report clock; the default aggregation interval is one second.
	 */
	TelemetrySystem::TelemetrySystem()
		: report_start_time(std::chrono::steady_clock::now())
	{
	}
	TelemetrySystem telemetry_system;

	/**
	 * @brief Resolves a hierarchical node, using a per-thread cache before locking the node table.
	 * @param name Stable scope-name pointer.
	 * @return Node ID for the (parent, name) pair, pushed onto this thread's scope stack.
	 */
	uint32 TelemetrySystem::beginScope(const char* name)
	{
		const uint32 parent_id = scope_stack.size() == 0 ? invalid_node_id : scope_stack.peek();
		for(const ScopeCacheEntry& cached : scope_cache)
		{
			if(cached.name == name && cached.parent_id == parent_id)
			{
				scope_stack.push(cached.node_id);
				return cached.node_id;
			}
		}
		uint32 node_id = invalid_node_id;
		{
			std::lock_guard<std::mutex> guard(mutex);
			for(uint32 index = 0; index < nodes.size(); index++)
			{
				const Node& node = nodes[index];
				if(node.parent_id == parent_id && std::strcmp(node.name, name) == 0)
				{
					node_id = index;
					break;
				}
			}
			if(node_id == invalid_node_id)
			{
				node_id = static_cast<uint32>(nodes.size());
				nodes.push(Node{name, parent_id});
			}
		}
		scope_cache.push(ScopeCacheEntry{name, parent_id, node_id});
		scope_stack.push(node_id);
		return node_id;
	}

	/**
	 * @brief Pops the matching thread-local scope and accumulates duration and call count.
	 * @param node_id Scope node previously returned by beginScope().
	 * @param elapsed_nanoseconds Elapsed steady-clock time to add to the current interval.
	 */
	void TelemetrySystem::endScope(uint32 node_id, uint64 elapsed_nanoseconds)
	{

		if(scope_stack.size() > 0 && scope_stack.peek() == node_id)
		{
			scope_stack.resize(scope_stack.size() - 1);
		}
		std::lock_guard<std::mutex> guard(mutex);
		if(node_id >= nodes.size()) return;
		nodes[node_id].tick_nanoseconds += elapsed_nanoseconds;
		nodes[node_id].tick_calls++;
	}

	/**
	 * @brief Marks the start of one frame for the over-30-ms stall threshold.
	 */
	void TelemetrySystem::beginTick()
	{
		tick_start_time = std::chrono::steady_clock::now();
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_frame = PlanetTerrainTreeTelemetry();
	}

	/**
	 * @brief Accumulates per-frame counters and conditionally logs the interval summary.
	 *
	 * The interval is considered slow when any frame exceeds 30 ms or when
	 * ticks / elapsed seconds is below 20. Totals and maxima reset after every
	 * interval regardless of whether a log is emitted. Report rows are grouped by
	 * parent scope and times include nested Watchers.
	 */
	void TelemetrySystem::endTick(bool force_report)
	{
		DynamicArray<Node> report_nodes;
		DynamicArray<PlanetMeshLevelTelemetry> report_mesh_levels;
		uint64 ticks_in_report = 0;
		double elapsed_milliseconds = 0.0;
		bool should_log_profile = false;
		bool should_log_telemetry = false;
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
			const auto now = std::chrono::steady_clock::now();
			if(now - tick_start_time > std::chrono::milliseconds(30)) report_has_slow_tick = true;
			report_tick_count++;
			for(Node& node : nodes)
			{
				node.total_nanoseconds += node.tick_nanoseconds;
				node.total_calls += node.tick_calls;
				if(node.tick_nanoseconds > node.maximum_tick_nanoseconds)
				{
					node.maximum_tick_nanoseconds = node.tick_nanoseconds;
				}
				node.tick_nanoseconds = 0;
				node.tick_calls = 0;
			}
			const auto elapsed_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
				now - report_start_time).count();
			elapsed_milliseconds = static_cast<double>(elapsed_nanoseconds) / 1000000.0;
			if(!force_report && elapsed_milliseconds < report_interval_milliseconds)
			{
				terrain_tree_snapshot = terrain_tree_frame;
				return;
			}

			ticks_in_report = report_tick_count;
			should_log_profile = force_report || report_has_slow_tick
				|| static_cast<double>(ticks_in_report) * 1000000000.0
					< static_cast<double>(elapsed_nanoseconds) * 40.0;
			if(should_log_profile) report_nodes = nodes;
			for(Node& node : nodes)
			{
				node.total_nanoseconds = 0;
				node.maximum_tick_nanoseconds = 0;
				node.total_calls = 0;
			}
			report_tick_count = 0;
			report_has_slow_tick = false;
			report_start_time = now;

			terrain_tree_snapshot = terrain_tree_frame;
			should_log_telemetry = has_telemetry_updates;
			if(should_log_telemetry)
			{
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
			}
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
			has_telemetry_updates = false;
		}

		if(should_log_telemetry)
		{
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
		if(!should_log_profile) return;

		formatLogInfo(
			"CPU profile: % ticks in % ms",
			ticks_in_report,
			elapsed_milliseconds);

		const size_t root = report_nodes.size();
		const size_t no_node = static_cast<size_t>(-1);
		DynamicArray<size_t> first_child(root + 1, no_node);
		DynamicArray<size_t> next_sibling(root, no_node);
		// Prepending in reverse preserves the order in which each parent's children were discovered.
		for(size_t index = root; index > 0;)
		{
			--index;
			const size_t parent_id = report_nodes[index].parent_id;
			const size_t parent = parent_id < root ? parent_id : root;
			next_sibling[index] = first_child[parent];
			first_child[parent] = index;
		}

		static constexpr char indentation[] = "                                ";
		constexpr size_t max_indent = sizeof(indentation) - 1;
		const double minimum_average_profile_time_ms =
			std::max(0.0, static_cast<double>(settings.minimum_average_profile_time_ms));
		const auto report_children = [&](const auto& report, size_t parent, size_t depth) -> void
		{
			for(size_t index = first_child[parent]; index != no_node; index = next_sibling[index])
			{
				const Node& node = report_nodes[index];
				const double average_milliseconds_per_tick =
					static_cast<double>(node.total_nanoseconds) / ticks_in_report / 1000000.0;
				const bool visible = node.total_calls > 0
					&& average_milliseconds_per_tick >= minimum_average_profile_time_ms;
				if(visible)
				{
					const size_t spaces = depth < max_indent / 2 ? depth * 2 : max_indent;
					formatLogInfo(
						"% %: avg % ms/tick, max % ms/tick, calls %",
						indentation + max_indent - spaces,
						node.name,
						average_milliseconds_per_tick,
						static_cast<double>(node.maximum_tick_nanoseconds) / 1000000.0,
						node.total_calls);
				}
				report(report, index, depth + (visible ? 1 : 0));
			}
		};
		report_children(report_children, root, 0);
	}

	/**
	 * @brief Sets the report interval, clamping zero to one millisecond.
	 * @param milliseconds New interval duration.
	 */
	void TelemetrySystem::setReportIntervalMilliseconds(uint64 milliseconds)
	{
		std::lock_guard<std::mutex> guard(mutex);
		report_interval_milliseconds = milliseconds == 0 ? 1 : milliseconds;
	}

	void TelemetrySystem::recordPlanetMeshGeneration(uint8 level, uint64 triangle_count,
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
		has_telemetry_updates = true;
	}

	void TelemetrySystem::recordPlanetMeshAcceptance(PlanetMeshAcceptance outcome)
	{
		std::lock_guard<std::mutex> guard(mutex);
		switch(outcome)
		{
			case PlanetMeshAcceptance::accepted: accepted_planet_meshes++; break;
			case PlanetMeshAcceptance::empty:
				empty_planet_meshes++;
				break;
			case PlanetMeshAcceptance::missing_planet: missing_planet_meshes++; break;
			case PlanetMeshAcceptance::stale_planet: stale_planet_meshes++; break;
			case PlanetMeshAcceptance::stale_terrain: stale_terrain_meshes++; break;
			case PlanetMeshAcceptance::invalid_node: invalid_planet_mesh_nodes++; break;
			case PlanetMeshAcceptance::stale_generator: stale_planet_mesh_generators++; break;
			case PlanetMeshAcceptance::stale_node: stale_planet_mesh_nodes++; break;
		}
		has_telemetry_updates = true;
	}

	void TelemetrySystem::recordPlanetMeshEviction(uint64 count)
	{
		if(count == 0) return;
		std::lock_guard<std::mutex> guard(mutex);
		evicted_planet_meshes += count;
		has_telemetry_updates = true;
	}

	void TelemetrySystem::setPlanetMeshQueueSnapshot(const PlanetMeshQueueTelemetry& snapshot)
	{
		std::lock_guard<std::mutex> guard(mutex);
		mesh_queue_snapshot = snapshot;
		if(snapshot.desired > 0 || snapshot.queued > 0 || snapshot.active > 0
			|| snapshot.completed > 0) has_telemetry_updates = true;
	}

	void TelemetrySystem::recordPlanetTerrainTree(const PlanetTerrainTreeTelemetry& snapshot)
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
		has_telemetry_updates = true;
	}

	void TelemetrySystem::recordPlanetTerrainCache(uint64 cached_meshes,
		uint64 active_meshes, uint64 cache_capacity)
	{
		std::lock_guard<std::mutex> guard(mutex);
		terrain_tree_frame.cached_meshes += cached_meshes;
		terrain_tree_frame.active_meshes += active_meshes;
		terrain_tree_frame.cache_capacity += cache_capacity;
		has_telemetry_updates = true;
	}

	/**
	 * @brief Begins a profiler timing scope and records its parent scope for hierarchical reporting.
	 */
	Watcher::Watcher(const char* name)
		: node_id(telemetry_system.beginScope(name == nullptr ? "(unnamed)" : name)),

		  start_time(std::chrono::steady_clock::now())
	{
	}

	/**
	 * @brief Records the elapsed time for the watched scope.
	 */
	Watcher::~Watcher()
	{
		const auto elapsed = std::chrono::steady_clock::now() - start_time;
		const uint64 elapsed_nanoseconds = static_cast<uint64>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
		telemetry_system.endScope(node_id, elapsed_nanoseconds);
	}
}
