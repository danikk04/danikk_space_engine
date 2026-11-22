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
		mesh_generation_telemetry.beginFrame();
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
		mesh_generation_telemetry.endFrame();
		DynamicArray<Node> report_nodes;
		uint64 ticks_in_report = 0;
		double elapsed_milliseconds = 0.0;
		bool should_log_profile = false;
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
				return;

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
