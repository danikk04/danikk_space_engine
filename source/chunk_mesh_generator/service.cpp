#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/chunk_mesh_generator.h>

#include "internal.h"

namespace danikk_space_engine
{
	namespace
	{
		constexpr size_t max_completed_meshes = 2;
		constexpr size_t max_worker_count = 4;

		bool sameRequestKey(const ChunkMeshGenerationRequest& a, const ChunkMeshGenerationRequest& b)
		{
			return a.generation_id == b.generation_id
				&& a.source == b.source
				&& a.source_revision == b.source_revision
				&& a.component_slot_id == b.component_slot_id
				&& a.component_generation_id == b.component_generation_id
				&& a.terrain_revision == b.terrain_revision
				&& a.world_body_id == b.world_body_id
				&& a.chunk_id == b.chunk_id
				&& a.grid_coordinate == b.grid_coordinate
				&& a.planet_mesh_generator == b.planet_mesh_generator
				&& a.planet_terrain == b.planet_terrain;
		}

		bool requestHasCompletedResult(
			const ChunkMeshGenerationRequest& request,
			const DynamicArray<GeneratedChunkMesh>& completed)
		{
			for(const GeneratedChunkMesh& result : completed)
			{
				if(result.generation_id == request.generation_id
					&& result.source == request.source
					&& result.source_revision == request.source_revision
					&& result.component_slot_id == request.component_slot_id
					&& result.component_generation_id == request.component_generation_id
					&& result.terrain_revision == request.terrain_revision
					&& result.world_body_id == request.world_body_id
					&& result.chunk_id == request.chunk_id
					&& result.grid_coordinate == request.grid_coordinate
					&& result.planet_mesh_generator == request.planet_mesh_generator) return true;
			}
			return false;
		}

		bool resultHasRequest(
			const GeneratedChunkMesh& result,
			const DynamicArray<ChunkMeshGenerationRequest>& requests)
		{
			for(const ChunkMeshGenerationRequest& request : requests)
			{
				if(result.generation_id == request.generation_id
					&& result.source == request.source
					&& result.source_revision == request.source_revision
					&& result.component_slot_id == request.component_slot_id
					&& result.component_generation_id == request.component_generation_id
					&& result.terrain_revision == request.terrain_revision
					&& result.world_body_id == request.world_body_id
					&& result.chunk_id == request.chunk_id
					&& result.grid_coordinate == request.grid_coordinate
					&& result.planet_mesh_generator == request.planet_mesh_generator) return true;
			}
			return false;
		}

		GeneratedChunkMesh generateChunkMesh(const ChunkMeshGenerationRequest& request)
		{
			switch(request.source)
			{
				case ChunkMeshSource::planet_terrain:
					return chunk_mesh_generator_internal::generatePlanetTerrainChunkMesh(request);
				case ChunkMeshSource::build_parts:
					return chunk_mesh_generator_internal::generateBuildPartChunkMesh(request);
			}
			danikk_engine::formatFatalError("Unsupported chunk mesh source %", static_cast<uint32>(request.source));
			return {};
		}

		class ChunkMeshGeneratorService
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
			bool stopping = false;
			uint64 current_generation_id = 0;

			size_t findActiveRequest(const ChunkMeshGenerationRequest& request) const
			{
				for(size_t index = 0; index < active_requests.size(); ++index)
					if(sameRequestKey(active_requests[index].request, request)) return index;
				return active_requests.size();
			}

			bool isActive(const ChunkMeshGenerationRequest& request) const
			{
				return findActiveRequest(request) < active_requests.size();
			}

			void removeActiveRequest(size_t index)
			{
				for(size_t shift = index + 1; shift < active_requests.size(); ++shift)
					active_requests[shift - 1] = static_cast<ActiveRequest&&>(active_requests[shift]);
				active_requests.resize(active_requests.size() - 1);
			}

			void run()
			{
				while(true)
				{
					ChunkMeshGenerationRequest request;
					{
						std::unique_lock<std::mutex> lock(mutex);
						request_available.wait(lock, [this] { return stopping || !requests.empty(); });
						if(stopping) return;
						request = static_cast<ChunkMeshGenerationRequest&&>(requests[0]);
						for(size_t i = 1; i < requests.size(); ++i)
							requests[i - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[i]);
						requests.resize(requests.size() - 1);
						active_requests.push(ActiveRequest{request, false});
					}

					GeneratedChunkMesh result = generateChunkMesh(request);
					{
						std::unique_lock<std::mutex> lock(mutex);
						result_space_available.wait(lock, [this, &request]
						{
							const size_t active_index = findActiveRequest(request);
							return stopping || completed.size() < max_completed_meshes
								|| active_index >= active_requests.size() || active_requests[active_index].cancelled;
						});
						const size_t active_index = findActiveRequest(request);
						const bool cancelled = active_index >= active_requests.size()
							|| active_requests[active_index].cancelled;
						if(active_index < active_requests.size()) removeActiveRequest(active_index);
						if(!stopping && !cancelled && request.generation_id == current_generation_id)
							completed.pushMove(result);
					}
				}
			}
		public:
			~ChunkMeshGeneratorService() { stop(); }

			void start()
			{
				std::lock_guard<std::mutex> guard(mutex);
				if(!workers.empty()) return;
				for(uint8 index = 0; index < static_cast<uint8>(BuildPartShape::count); ++index)
					getBuildPartGeometry(static_cast<BuildPartShape>(index));
				stopping = false;
				const size_t available_threads = std::thread::hardware_concurrency();
				const size_t worker_count = available_threads <= 1
					? 1 : std::min(available_threads - 1, max_worker_count);
				for(size_t index = 0; index < worker_count; ++index)
					workers.pushMove(std::thread(&ChunkMeshGeneratorService::run, this));
			}

			void stop()
			{
				{
					std::lock_guard<std::mutex> guard(mutex);
					if(workers.empty()) return;
					stopping = true;
				}
				request_available.notify_all();
				result_space_available.notify_all();
				for(std::thread& worker : workers)
					if(worker.joinable()) worker.join();
				std::lock_guard<std::mutex> guard(mutex);
				requests.clear();
				completed.clear();
				active_requests.clear();
				workers.clear();
				stopping = false;
			}

			void update(uint64 generation_id, const DynamicArray<ChunkMeshGenerationRequest>& new_requests)
			{
				{
					std::lock_guard<std::mutex> guard(mutex);
					if(current_generation_id != generation_id)
					{
						current_generation_id = generation_id;
						completed.clear();
						result_space_available.notify_all();
					}

					for(size_t index = 0; index < completed.size();)
					{
						if(resultHasRequest(completed[index], new_requests))
						{
							++index;
							continue;
						}
						for(size_t shift = index + 1; shift < completed.size(); ++shift)
							completed[shift - 1] = static_cast<GeneratedChunkMesh&&>(completed[shift]);
						completed.resize(completed.size() - 1);
					}
					for(ActiveRequest& active : active_requests)
					{
						bool still_requested = false;
						for(const ChunkMeshGenerationRequest& request : new_requests)
						{
							if(sameRequestKey(active.request, request))
							{
								still_requested = true;
								break;
							}
						}
						if(!still_requested) active.cancelled = true;
					}
					result_space_available.notify_all();

				size_t queue_index = 0;
				bool same_queue = true;
				for(const ChunkMeshGenerationRequest& request : new_requests)
				{
					if(request.generation_id != current_generation_id
						|| isActive(request)
						|| requestHasCompletedResult(request, completed)) continue;
					if(queue_index >= requests.size() || !sameRequestKey(requests[queue_index], request))
					{
						same_queue = false;
						break;
					}
					queue_index++;
				}
				if(same_queue && queue_index == requests.size()) return;

				requests.clear();
				for(const ChunkMeshGenerationRequest& request : new_requests)
				{
					if(request.generation_id != current_generation_id
						|| isActive(request)
						|| requestHasCompletedResult(request, completed)) continue;
					requests.push(request);
				}
				}
				request_available.notify_all();
			}

			bool tryTake(GeneratedChunkMesh& result)
			{
				std::lock_guard<std::mutex> guard(mutex);
				while(!completed.empty())
				{
					GeneratedChunkMesh candidate = static_cast<GeneratedChunkMesh&&>(completed[0]);
					for(size_t index = 1; index < completed.size(); ++index)
						completed[index - 1] = static_cast<GeneratedChunkMesh&&>(completed[index]);
					completed.resize(completed.size() - 1);
					if(candidate.generation_id != current_generation_id) continue;
					result = static_cast<GeneratedChunkMesh&&>(candidate);
					result_space_available.notify_one();
					return true;
				}
				return false;
			}

			void cancel(WorldBodyComponentSlotId component_slot_id, uint64 component_generation_id)
			{
				std::lock_guard<std::mutex> guard(mutex);
				for(size_t index = 0; index < requests.size();)
				{
					const ChunkMeshGenerationRequest& request = requests[index];
					if(request.source != ChunkMeshSource::planet_terrain
						|| request.component_slot_id != component_slot_id
						|| request.component_generation_id != component_generation_id)
					{
						++index;
						continue;
					}
					for(size_t shift = index + 1; shift < requests.size(); ++shift)
						requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
					requests.resize(requests.size() - 1);
				}
				for(size_t index = 0; index < completed.size();)
				{
					const GeneratedChunkMesh& result = completed[index];
					if(result.source != ChunkMeshSource::planet_terrain
						|| result.component_slot_id != component_slot_id
						|| result.component_generation_id != component_generation_id)
					{
						++index;
						continue;
					}
					for(size_t shift = index + 1; shift < completed.size(); ++shift)
						completed[shift - 1] = static_cast<GeneratedChunkMesh&&>(completed[shift]);
					completed.resize(completed.size() - 1);
				}
				for(ActiveRequest& active : active_requests)
					if(active.request.source == ChunkMeshSource::planet_terrain
						&& active.request.component_slot_id == component_slot_id
						&& active.request.component_generation_id == component_generation_id)
						active.cancelled = true;
				request_available.notify_all();
				result_space_available.notify_all();
			}
		};

		ChunkMeshGeneratorService generator_service;
	}

	void startChunkMeshGenerator() { generator_service.start(); }
	void stopChunkMeshGenerator() { generator_service.stop(); }
	void updateChunkMeshGenerationRequests(uint64 generation_id,
		const DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		DSE_WATCH();
		generator_service.update(generation_id, requests);
	}
	void cancelChunkMeshGenerationRequests(
		WorldBodyComponentSlotId component_slot_id, uint64 component_generation_id)
	{
		generator_service.cancel(component_slot_id, component_generation_id);
	}
	bool tryTakeGeneratedChunkMesh(GeneratedChunkMesh& result)
	{
		return generator_service.tryTake(result);
	}
}
