#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>

#include <danikk_framework/thread.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine::mesh_generation_internal
{
	GeneratedMesh generatePlanetTerrainNodeMesh(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generatePlanetTerrainNodeMarchingTetrahedra(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generatePlanetTerrainNodeSurfaceNets(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generateBuildPartMesh(World& world, const MeshGenerationRequest& request);

	/** @brief CPU worker queue owned by one MeshGenerationSystem instance. */
	class Worker
	{
		World& world;
		struct ActiveRequest
		{
			MeshGenerationRequest request;
			bool cancelled = false;
		};

		std::mutex mutex;
		std::condition_variable result_space_available;
		std::unique_ptr<danikk_framework::ThreadPool> thread_pool;
		DynamicArray<MeshGenerationRequest> requests;
		DynamicArray<GeneratedMesh> completed;
		DynamicArray<ActiveRequest> active_requests;
		uint32 generation_sample_count = 0;
		uint64 generation_sample_total_nanoseconds = 0;
		std::atomic<float> average_generation_milliseconds = 0.0f;
		bool stopping = false;
		uint64 current_generation_id = 0;

		size_t findActiveRequest(const MeshGenerationRequest& request) const;
		bool isActive(const MeshGenerationRequest& request) const;
		void removeActiveRequest(size_t index);
		void dispatchPendingRequestsLocked();
		void generateRequest(const MeshGenerationRequest& request);

	public:
		explicit Worker(World& world) : world(world) {}
		~Worker();
		void start();
		void stop();
		size_t getWorkerCount() const { return thread_pool != nullptr ? thread_pool->workerCount() : 0; }
		void update(uint64 generation_id, const DynamicArray<MeshGenerationRequest>& requests);
		bool tryTake(GeneratedMesh& result);
		bool getAverageGenerationTimeMilliseconds(float& average_time) const;
		void cancel(PlanetId planet_id, uint64 planet_generation_id);
	};
}
