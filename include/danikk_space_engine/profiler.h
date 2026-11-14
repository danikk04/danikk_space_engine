#pragma once

#include <chrono>
#include <mutex>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	class Profiler
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

		std::mutex mutex;
		danikk_framework::DynamicArray<Node> nodes;
		uint64 report_tick_count = 0;
		uint64 report_interval_milliseconds = 1000;
		std::chrono::steady_clock::time_point report_start_time;

		uint32 beginScope(const char* name);
		void endScope(uint32 node_id, uint64 elapsed_nanoseconds);
		friend class Watcher;
	public:
		Profiler();
		Profiler(const Profiler&) = delete;
		Profiler& operator=(const Profiler&) = delete;

		void beginTick();
		void endTick();
		void setReportIntervalMilliseconds(uint64 milliseconds);
	};

	class Watcher
	{
		uint32 node_id = static_cast<uint32>(-1);
		std::chrono::steady_clock::time_point start_time;

	public:
		// The profiler retains this name pointer for reports; pass a string with static lifetime.
		explicit Watcher(const char* name);
		Watcher(const Watcher&) = delete;
		Watcher& operator=(const Watcher&) = delete;
		~Watcher();
	};

	extern Profiler profiler;
}
