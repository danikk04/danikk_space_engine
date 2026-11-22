#include <atomic>
#include <cstring>

#include <danikk_framework/log.h>

#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/telemetry/profiling.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	namespace
	{
		constexpr uint32 invalid_node_id = static_cast<uint32>(-1);
		std::atomic<ProfilingSystem*> active_profiling_system = nullptr;
	}

	ProfilingSystem::ProfilingSystem()
		: main_thread_id(std::this_thread::get_id()),
		  report_start_time(std::chrono::steady_clock::now())
	{
	}

	ProfilingSystem::~ProfilingSystem()
	{
		ProfilingSystem* active = this;
		active_profiling_system.compare_exchange_strong(active, nullptr, std::memory_order_release,
			std::memory_order_relaxed);
	}

	ProfilingSystem* ProfilingSystem::getCurrentThreadSystem()
	{
		ProfilingSystem* system = active_profiling_system.load(std::memory_order_acquire);
		return system != nullptr && std::this_thread::get_id() == system->main_thread_id
			? system : nullptr;
	}

	uint32 ProfilingSystem::beginScope(const char* name)
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
		scope_cache.push(ScopeCacheEntry{name, parent_id, node_id});
		scope_stack.push(node_id);
		return node_id;
	}

	void ProfilingSystem::endScope(uint32 node_id, uint64 elapsed_nanoseconds)
	{
		if(scope_stack.size() > 0 && scope_stack.peek() == node_id)
			scope_stack.resize(scope_stack.size() - 1);

		if(node_id >= nodes.size()) return;
		nodes[node_id].tick_nanoseconds += elapsed_nanoseconds;
		nodes[node_id].tick_calls++;
	}

	void ProfilingSystem::beginTick()
	{
		if(std::this_thread::get_id() != main_thread_id) return;
		active_profiling_system.store(this, std::memory_order_release);
		tick_start_time = std::chrono::steady_clock::now();
	}

	void ProfilingSystem::endTick(bool force_report)
	{
		if(std::this_thread::get_id() != main_thread_id) return;
		ProfilingSystem* active = this;
		active_profiling_system.compare_exchange_strong(active, nullptr,
			std::memory_order_release, std::memory_order_relaxed);
		DynamicArray<Node> report_nodes;
		uint64 ticks_in_report = 0;
		double elapsed_milliseconds = 0.0;
		bool should_log_profile = false;
		{
			const auto now = std::chrono::steady_clock::now();
			if(now - tick_start_time > std::chrono::milliseconds(30)) report_has_slow_tick = true;
			report_tick_count++;
			for(Node& node : nodes)
			{
				node.total_nanoseconds += node.tick_nanoseconds;
				node.total_calls += node.tick_calls;
				if(node.tick_nanoseconds > node.maximum_tick_nanoseconds)
					node.maximum_tick_nanoseconds = node.tick_nanoseconds;
				node.tick_nanoseconds = 0;
				node.tick_calls = 0;
			}
			const auto elapsed_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
				now - report_start_time).count();
			elapsed_milliseconds = static_cast<double>(elapsed_nanoseconds) / 1000000.0;
			if(!force_report && elapsed_milliseconds < report_interval_milliseconds) return;

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

		formatLogInfo("CPU profile: % ticks in % ms", ticks_in_report, elapsed_milliseconds);

		const size_t root = report_nodes.size();
		const size_t no_node = static_cast<size_t>(-1);
		DynamicArray<size_t> first_child(root + 1, no_node);
		DynamicArray<size_t> next_sibling(root, no_node);
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
					formatLogInfo("% %: avg % ms/tick, max % ms/tick, calls %",
						indentation + max_indent - spaces, node.name,
						average_milliseconds_per_tick,
						static_cast<double>(node.maximum_tick_nanoseconds) / 1000000.0,
						node.total_calls);
				}
				report(report, index, depth + (visible ? 1 : 0));
			}
		};
		report_children(report_children, root, 0);
	}

	void ProfilingSystem::setReportIntervalMilliseconds(uint64 milliseconds)
	{
		if(std::this_thread::get_id() != main_thread_id) return;
		report_interval_milliseconds = milliseconds == 0 ? 1 : milliseconds;
	}
}
