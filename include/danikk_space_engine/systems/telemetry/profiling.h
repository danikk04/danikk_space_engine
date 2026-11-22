#pragma once

#include <chrono>
#include <thread>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	class Watcher;

	/** @brief Collects hierarchical CPU timings and reports slow-frame profiles. */
	class ProfilingSystem
	{
		struct Node
		{
			const char* name = nullptr;
			uint32 parent_id = static_cast<uint32>(-1);
			uint64 tick_nanoseconds = 0;
			uint64 tick_calls = 0;
			uint64 total_nanoseconds = 0;
			uint64 maximum_tick_nanoseconds = 0;
			uint64 total_calls = 0;
		};
		struct ScopeCacheEntry
		{
			const char* name = nullptr;
			uint32 parent_id = static_cast<uint32>(-1);
			uint32 node_id = static_cast<uint32>(-1);
		};

		danikk_framework::DynamicArray<Node> nodes;
		danikk_framework::DynamicArray<uint32> scope_stack;
		danikk_framework::DynamicArray<ScopeCacheEntry> scope_cache;
		uint64 report_tick_count = 0;
		uint64 report_interval_milliseconds = 1000;
		bool report_has_slow_tick = false;
		std::thread::id main_thread_id;
		std::chrono::steady_clock::time_point tick_start_time;
		std::chrono::steady_clock::time_point report_start_time;

		uint32 beginScope(const char* name);
		void endScope(uint32 node_id, uint64 elapsed_nanoseconds);
		friend class Watcher;

	public:
		ProfilingSystem();
		~ProfilingSystem();
		ProfilingSystem(const ProfilingSystem&) = delete;
		ProfilingSystem& operator=(const ProfilingSystem&) = delete;

		/** @brief Records the start time of the frame used for stall detection. */
		void beginTick();
		/** @brief Accumulates this frame and conditionally emits the CPU profile. */
		void endTick(bool force_report = false);
		/** @brief Changes the aggregation and logging interval. */
		void setReportIntervalMilliseconds(uint64 milliseconds);
	};
}
