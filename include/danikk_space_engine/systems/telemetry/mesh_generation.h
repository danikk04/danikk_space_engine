#pragma once

#include <mutex>

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/config.h>

namespace danikk_space_engine
{
	struct TelemetryTimingStats
	{
		uint64 samples = 0;
		uint64 total_nanoseconds = 0;
		uint64 minimum_nanoseconds = 0;
		uint64 maximum_nanoseconds = 0;
	};

	struct PlanetMeshLevelTelemetry
	{
		uint64 generated_meshes = 0;
		uint64 generated_triangles = 0;
		TelemetryTimingStats generation_time;
	};

	struct PlanetMeshQueueTelemetry
	{
		uint64 queued = 0;
		uint64 fine_queued = 0;
		uint64 active = 0;
		uint64 fine_active = 0;
		uint64 completed = 0;
	};

	struct PlanetTerrainTreeTelemetry
	{
		uint64 planets = 0;
		uint64 nodes = 0;
		uint64 active_nodes = 0;
		uint64 target_nodes = 0;
		uint64 detail_scale_adjustments = 0;
		uint64 generation_pending_nodes = 0;
		uint64 queued_jobs = 0;
		uint64 fine_queued = 0;
		uint64 cached_meshes = 0;
		uint64 active_meshes = 0;
		uint64 cache_capacity = 0;
		double finest_job_width = 0.0;
		double finest_active_half_extent = 0.0;
		double detail_scale_total = 0.0;
	};

	enum class PlanetMeshAcceptance : uint8
	{
		accepted,
		empty,
		missing_planet,
		stale_terrain,
		invalid_node,
		stale_generator,
		stale_node
	};

	enum class MeshDeletionReason : uint8
	{
		cache_capacity,
		tree_node_limit,
		planet_removed,
		planet_system_shutdown,
		planet_render_unloaded,
		detail_mesh_rebuild,
		world_body_destroyed
	};

	/** @brief Collects and explicitly reports mesh generation, streaming, and cache telemetry. */
	class MeshGenerationTelemetry
	{
		struct MeshDeletionCounts
		{
			uint64 total = 0;
			uint64 cache_capacity = 0;
			uint64 tree_node_limit = 0;
			uint64 planet_removed = 0;
			uint64 planet_system_shutdown = 0;
			uint64 planet_render_unloaded = 0;
			uint64 detail_mesh_rebuild = 0;
			uint64 world_body_destroyed = 0;
		};

		std::mutex mutex;
		danikk_framework::DynamicArray<PlanetMeshLevelTelemetry> planet_mesh_levels;
		uint64 accepted_planet_meshes = 0;
		uint64 empty_planet_meshes = 0;
		uint64 missing_planet_meshes = 0;
		uint64 stale_terrain_meshes = 0;
		uint64 invalid_planet_mesh_nodes = 0;
		uint64 stale_planet_mesh_generators = 0;
		uint64 stale_planet_mesh_nodes = 0;
		uint64 evicted_planet_meshes = 0;
		MeshDeletionCounts pending_mesh_deletions;
		PlanetMeshQueueTelemetry mesh_queue_snapshot;
		PlanetTerrainTreeTelemetry terrain_tree_frame;
		PlanetTerrainTreeTelemetry terrain_tree_snapshot;
		bool has_updates = false;

	public:
		void beginFrame();
		void endFrame();
		void recordPlanetMeshGeneration(uint8 level, uint64 triangle_count, uint64 elapsed_nanoseconds);
		void recordPlanetMeshAcceptance(PlanetMeshAcceptance outcome);
		void recordPlanetMeshEviction(uint64 count);
		void recordMeshDeletion(MeshDeletionReason reason);
		void recordPlanetTerrainCache(uint64 cached_meshes, uint64 active_meshes, uint64 cache_capacity);
		void setPlanetMeshQueueSnapshot(const PlanetMeshQueueTelemetry& snapshot);
		void recordPlanetTerrainTree(const PlanetTerrainTreeTelemetry& snapshot);
		void report();
	};
}
