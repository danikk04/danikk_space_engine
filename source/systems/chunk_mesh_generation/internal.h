#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	GeneratedChunkMesh generatePlanetSurfaceNodeMesh(World& world, const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetChunkMarchingTetrahedra(World& world, const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetTerrainChunkMeshSurfaceNets(World& world, const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generateBuildPartChunkMesh(World& world, const ChunkMeshGenerationRequest& request);

	/** @brief CPU worker queue owned by one ChunkMeshGenerationSystem instance. */
	class Worker
	{
		World& world;
		struct ActiveRequest
		{
			ChunkMeshGenerationRequest request;
			bool cancelled = false;
		};

		std::mutex mutex;
		std::condition_variable request_available;
		std::condition_variable result_space_available;
		DynamicArray<ChunkMeshGenerationRequest> requests;
		DynamicArray<GeneratedChunkMesh> completed;
		DynamicArray<std::thread> workers;
		DynamicArray<ActiveRequest> active_requests;
		uint32 generation_sample_count = 0;
		uint64 generation_sample_total_nanoseconds = 0;
		uint64 generation_sample_minimum_nanoseconds = 0;
		uint64 generation_sample_maximum_nanoseconds = 0;
		std::atomic<float> average_generation_milliseconds = 0.0f;
		bool stopping = false;
		uint64 current_generation_id = 0;

		size_t findActiveRequest(const ChunkMeshGenerationRequest& request) const;
		bool isActive(const ChunkMeshGenerationRequest& request) const;
		void removeActiveRequest(size_t index);
		void run();

	public:
		explicit Worker(World& world) : world(world) {}
		~Worker();
		void start();
		void stop();
		void update(uint64 generation_id, const DynamicArray<ChunkMeshGenerationRequest>& requests);
		bool tryTake(GeneratedChunkMesh& result);
		bool getAverageGenerationTimeMilliseconds(float& average_time) const;
		void cancel(WorldBodyId world_body_id, uint64 planet_generation_id);
	};
}
