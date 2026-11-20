#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	GeneratedChunkMesh generatePlanetTerrainChunkMesh(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetChunkMarchingTetrahedra(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetSurfaceLodMesh(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetTerrainChunkMeshSurfaceNets(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generateBuildPartChunkMesh(const ChunkMeshGenerationRequest& request);

	/** @brief CPU worker queue owned by one ChunkMeshGenerationSystem instance. */
	class Worker
	{
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
		bool stopping = false;
		uint64 current_generation_id = 0;

		size_t findActiveRequest(const ChunkMeshGenerationRequest& request) const;
		bool isActive(const ChunkMeshGenerationRequest& request) const;
		void removeActiveRequest(size_t index);
		void run();

	public:
		~Worker();
		void start();
		void stop();
		void update(uint64 generation_id, const DynamicArray<ChunkMeshGenerationRequest>& requests);
		bool tryTake(GeneratedChunkMesh& result);
		void cancel(WorldBodyId world_body_id, uint64 planet_generation_id);
	};
}
