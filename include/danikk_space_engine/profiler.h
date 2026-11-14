#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	using ProfilerUInt32 = std::uint32_t;
	using ProfilerUInt64 = std::uint64_t;

	class Profiler
	{
		struct Node
		{
			const char* name = nullptr;
			ProfilerUInt32 parent_id = static_cast<ProfilerUInt32>(-1);
			ProfilerUInt64 tick_nanoseconds = 0;
			ProfilerUInt64 tick_calls = 0;
			ProfilerUInt64 total_nanoseconds = 0;
			ProfilerUInt64 maximum_tick_nanoseconds = 0;
			ProfilerUInt64 total_calls = 0;
		};

		std::mutex mutex;
		danikk_framework::DynamicArray<Node> nodes;
		ProfilerUInt64 report_tick_count = 0;
		ProfilerUInt64 report_interval_milliseconds = 1000;
		std::chrono::steady_clock::time_point report_start_time;

		ProfilerUInt32 beginScope(const char* name);
		void endScope(ProfilerUInt32 node_id, ProfilerUInt64 elapsed_nanoseconds);
		friend class Watcher;
	public:
		Profiler();
		Profiler(const Profiler&) = delete;
		Profiler& operator=(const Profiler&) = delete;

		void beginTick();
		void endTick();
		void setReportIntervalMilliseconds(ProfilerUInt64 milliseconds);
	};

	class Watcher
	{
		ProfilerUInt32 node_id = static_cast<ProfilerUInt32>(-1);
		std::chrono::steady_clock::time_point start_time;

	public:
		explicit Watcher(const char* name);
		Watcher(const Watcher&) = delete;
		Watcher& operator=(const Watcher&) = delete;
		~Watcher();
	};

	extern Profiler profiler;
}
