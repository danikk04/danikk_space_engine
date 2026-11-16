#pragma once

#include <chrono>
#include <mutex>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	/**
	 * @brief Collects hierarchical CPU timings and reports intervals containing a performance stall.
	 *
	 * The global instance aggregates each Watcher by its name and parent scope.
	 * At each report interval it logs only when at least one frame exceeded 30 ms
	 * or the observed frame rate was below 20 FPS. Scope times include nested
	 * scopes, so parent and child values are not independent costs.
	 */
	class Profiler
	{

		/**
		 * @brief Accumulates timing and call-count statistics for one profiler scope.
		 */
		struct Node
		{
			/** @brief Stable scope label; the caller must keep its storage alive for the profiler lifetime. */
			const char* name = nullptr;
			/** @brief Parent node ID, or uint32(-1) for a top-level scope. */
			uint32 parent_id = static_cast<uint32>(-1);
			/** @brief Inclusive nanoseconds accumulated since the current frame began. */
			uint64 tick_nanoseconds = 0;
			/** @brief Number of completed calls accumulated since the current frame began. */
			uint64 tick_calls = 0;
			/** @brief Inclusive nanoseconds accumulated for the current report interval. */
			uint64 total_nanoseconds = 0;
			/** @brief Largest inclusive duration of this scope in any frame of the current interval. */
			uint64 maximum_tick_nanoseconds = 0;
			/** @brief Total completed calls accumulated for the current report interval. */
			uint64 total_calls = 0;
		};

		/** @brief Protects the shared node table and timing counters from worker-scope completion. */
		std::mutex mutex;
		/** @brief Scope nodes indexed by the IDs stored on thread-local scope stacks. */
		danikk_framework::DynamicArray<Node> nodes;
		/** @brief Number of frames accumulated since the start of the current report interval. */
		uint64 report_tick_count = 0;
		/** @brief Wall-clock duration that triggers an interval report, in milliseconds. */
		uint64 report_interval_milliseconds = 1000;
		/** @brief Whether any frame in the current interval exceeded the slow-frame threshold. */
		bool report_has_slow_tick = false;
		/** @brief Start timestamp of the frame currently being measured. */
		std::chrono::steady_clock::time_point tick_start_time;
		/** @brief Start timestamp of the current report interval. */
		std::chrono::steady_clock::time_point report_start_time;

		/**
		 * @brief Finds or creates a node beneath the current thread-local parent scope.
		 * @param name Stable-lifetime scope name retained by the report node.
		 * @return Profiler node ID pushed on the current thread's scope stack.
		 */
		uint32 beginScope(const char* name);

		/**
		 * @brief Adds one completed scope duration to its per-frame counters.
		 * @param node_id Node returned by beginScope().
		 * @param elapsed_nanoseconds Measured wall-clock duration.
		 */
		void endScope(uint32 node_id, uint64 elapsed_nanoseconds);
		friend class Watcher;
	public:
		Profiler();
		/** @brief Disabled so the process-wide profiler state cannot be accidentally copied. */
		Profiler(const Profiler&) = delete;
		/** @brief Disabled for the same singleton-state ownership reason as copy construction. */
		Profiler& operator=(const Profiler&) = delete;

		/**
		 * @brief Records the start time of the frame used for stall detection.
		 * @note Per-scope counters are rolled over in endTick(), not here.
		 */
		void beginTick();

		/**
		 * @brief Accumulates this frame and emits a hierarchical report at interval boundaries.
		 *
		 * Scope counters are aggregated under a mutex. Once the interval elapses,
		 * the report is suppressed unless a frame took over 30 ms or average FPS
		 * was below 20. Interval counters reset even when logging is suppressed.
		 */
		void endTick();

		/**
		 * @brief Changes the aggregation and logging interval.
		 * @param milliseconds Requested interval; zero is clamped to 1 ms.
		 */
		void setReportIntervalMilliseconds(uint64 milliseconds);
	};

	/**
	 * @brief Measures one named profiler scope and records its elapsed duration when it is destroyed.
	 */
	class Watcher
	{
		/** @brief Profiler node receiving this watcher's accumulated duration. */
		uint32 node_id = static_cast<uint32>(-1);
		/** @brief Monotonic timestamp captured when the watched scope begins. */
		std::chrono::steady_clock::time_point start_time;

	public:
		/**
		 * @brief Starts timing a named scope and registers it beneath the current parent scope.
		 * @param name Name retained for logging; it must have static or otherwise
		 *             sufficiently long lifetime because the profiler stores its pointer.
		 * A null name is recorded as `"(unnamed)"`. Watcher is non-copyable so a
		 * scope is ended exactly once by its destructor.
		 */
		explicit Watcher(const char* name);
		/** @brief Disabled so the same timed scope cannot be ended by multiple destructors. */
		Watcher(const Watcher&) = delete;
		/** @brief Disabled so one measured scope cannot be ended more than once. */
		Watcher& operator=(const Watcher&) = delete;
		/** @brief Stops the timer and adds its elapsed duration to the global profiler. */
		~Watcher();
	};

	/** @brief Process-wide profiler instance used by all Watcher objects. */
	extern Profiler profiler;
}
