#include <cstring>

#include <danikk_framework/log.h>

#include <danikk_space_engine/profiler.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	namespace
	{
		constexpr uint32 invalid_node_id = static_cast<uint32>(-1);
		thread_local DynamicArray<uint32> scope_stack;
		struct ScopeCacheEntry
		{
			const char* name = nullptr;
			uint32 parent_id = invalid_node_id;
			uint32 node_id = invalid_node_id;
		};
		thread_local danikk_framework::DynamicArray<ScopeCacheEntry> scope_cache;
	}

	Profiler::Profiler()
		: report_start_time(std::chrono::steady_clock::now())
	{
	}

	Profiler profiler;

	uint32 Profiler::beginScope(const char* name)
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

	void Profiler::endScope(uint32 node_id, uint64 elapsed_nanoseconds)
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

	void Profiler::beginTick()
	{
		// Tick counters are cleared by endTick so worker-thread scopes finishing between
		// consecutive main-thread frames are included in the next report interval.
		tick_start_time = std::chrono::steady_clock::now();
	}

	// Accumulates scope totals for the reporting interval and records any tick over 30 ms.
	// At the interval boundary, logs only if FPS is below 20 or a slow tick occurred.
	// Every interval resets its counters, including intervals whose reports are suppressed.
	void Profiler::endTick()
	{
		DynamicArray<Node> report_nodes;
		uint64 ticks_in_report = 0;
		uint64 elapsed_milliseconds = 0;
		bool should_log = false;
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
			elapsed_milliseconds = static_cast<uint64>(
				std::chrono::duration_cast<std::chrono::milliseconds>(now - report_start_time).count());
			if(elapsed_milliseconds < report_interval_milliseconds) return;

			ticks_in_report = report_tick_count;
			const auto elapsed_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
				now - report_start_time).count();
			should_log = report_has_slow_tick
				|| static_cast<double>(ticks_in_report) * 1000000000.0
					< static_cast<double>(elapsed_nanoseconds) * 20.0;
			if(should_log) report_nodes = nodes;
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
		if(!should_log) return;

		formatLogInfo(
			"CPU profile: % ticks in % ms",
			ticks_in_report,
			elapsed_milliseconds);
		static const char* indentation[] = {"", "  ", "    ", "      ", "        ", "          ", "            ", "              "};
		for(const Node& node : report_nodes)
		{
			if(node.total_calls == 0) continue;
			uint32 depth = 0;
			uint32 parent_id = node.parent_id;
			while(parent_id != invalid_node_id && parent_id < report_nodes.size())
			{
				depth++;
				parent_id = report_nodes[parent_id].parent_id;
			}
			if(depth >= sizeof(indentation) / sizeof(indentation[0]))
			{
				depth = sizeof(indentation) / sizeof(indentation[0]) - 1;
			}
			formatLogInfo(
				"% %: avg % ns/tick, max % ns/tick, calls %",
				indentation[depth],
				node.name,
				node.total_nanoseconds / ticks_in_report,
				node.maximum_tick_nanoseconds,
				node.total_calls);
		}
	}

	void Profiler::setReportIntervalMilliseconds(uint64 milliseconds)
	{
		std::lock_guard<std::mutex> guard(mutex);
		report_interval_milliseconds = milliseconds == 0 ? 1 : milliseconds;
	}

	Watcher::Watcher(const char* name)
		: node_id(profiler.beginScope(name == nullptr ? "(unnamed)" : name)),
		  start_time(std::chrono::steady_clock::now())
	{
	}

	Watcher::~Watcher()
	{
		const auto elapsed = std::chrono::steady_clock::now() - start_time;
		const uint64 elapsed_nanoseconds = static_cast<uint64>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
		profiler.endScope(node_id, elapsed_nanoseconds);
	}
}
