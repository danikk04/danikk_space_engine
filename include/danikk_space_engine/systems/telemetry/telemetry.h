#pragma once

#include <danikk_space_engine/systems/telemetry/mesh_generation.h>
#include <danikk_space_engine/systems/telemetry/profiling.h>
#include <danikk_space_engine/systems/telemetry/watcher.h>

namespace danikk_space_engine
{
	using namespace danikk_framework;

	/**
	 * @brief Provides access to profiling and mesh-generation telemetry subsystems.
	 *
	 * Frame lifecycle calls are forwarded to both child subsystems. CPU profile
	 * reports are emitted only when a frame exceeds 30 ms or average FPS is below
	 * 40; nested scope times include child scopes.
	 */
	class TelemetrySystem
	{

		/** @brief CPU function timing and hierarchical report subsystem. */
		ProfilingSystem profiling_system;
		/** @brief Mesh-specific counters, snapshots, and event-triggered logging. */
		MeshGenerationTelemetry mesh_generation_telemetry;
	public:
		TelemetrySystem();
		/** @brief Disabled so process-wide telemetry state cannot be accidentally copied. */
		TelemetrySystem(const TelemetrySystem&) = delete;
		/** @brief Disabled for the same singleton-state ownership reason as copy construction. */
		TelemetrySystem& operator=(const TelemetrySystem&) = delete;

		/**
		 * @brief Records the start time of the frame used for stall detection.
		 * @note Per-scope counters are rolled over in endTick(), not here.
		 */
		void beginTick();

		/**
		 * @brief Accumulates this frame and emits a CPU profile at interval boundaries.
		 * @param force_report Flushes an explicit measurement interval regardless of wall time or frame rate.
		 *
		 * Mesh telemetry is accumulated independently and can be explicitly reported
		 * with reportMeshGenerationTelemetry(). Scope counters are aggregated under a mutex.
		 * Once the interval elapses,
		 * the report is suppressed unless a frame took over 30 ms or average FPS
		 * was below 40. Interval counters reset even when logging is suppressed.
		 */
		void endTick(bool force_report = false);
		/** @brief Emits the current mesh telemetry snapshot when an external condition requests it. */
		void reportMeshGenerationTelemetry();

		/**
		 * @brief Changes the aggregation and logging interval.
		 * @param milliseconds Requested interval; zero is clamped to 1 ms.
		 */
		void setReportIntervalMilliseconds(uint64 milliseconds);
		/** @brief Provides access to the CPU profiling subsystem. */
		ProfilingSystem& getProfilingSystem() { return profiling_system; }
		/** @brief Provides read-only access to the CPU profiling subsystem. */
		const ProfilingSystem& getProfilingSystem() const { return profiling_system; }
		/** @brief Records a completed planet terrain mesh generation for its octree level. */
		void recordPlanetMeshGeneration(uint8 level, uint64 triangle_count, uint64 elapsed_nanoseconds);
		/** @brief Records why a generated planet terrain mesh was accepted or discarded. */
		void recordPlanetMeshAcceptance(PlanetMeshAcceptance outcome);
		/** @brief Records eviction of one cached planet terrain mesh. */
		void recordPlanetMeshEviction(uint64 count = 1);
		/** @brief Adds one planet's cached and active mesh counts to the current frame. */
		void recordPlanetTerrainCache(uint64 cached_meshes, uint64 active_meshes, uint64 cache_capacity);
		/** @brief Replaces the latest aggregate worker queue snapshot. */
		void setPlanetMeshQueueSnapshot(const PlanetMeshQueueTelemetry& snapshot);
		/** @brief Adds this planet's tree statistics to the current frame's aggregate. */
		void recordPlanetTerrainTree(const PlanetTerrainTreeTelemetry& snapshot);
	};

}

