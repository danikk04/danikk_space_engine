#include <algorithm>
#include <chrono>

#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>
#include <danikk_space_engine/profiler.h>

#include "internal.h"

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	namespace
	{
		constexpr size_t max_completed_meshes = 2;
		constexpr size_t max_worker_count = 4;
		constexpr bool log_planet_mesh_queue_diagnostics = true;

		bool sameRequestKey(const ChunkMeshGenerationRequest& a, const ChunkMeshGenerationRequest& b)
		{
			return a.generation_id == b.generation_id
				&& a.source == b.source
				&& a.source_revision == b.source_revision
				&& a.planet_generation_id == b.planet_generation_id
				&& a.terrain_revision == b.terrain_revision
				&& a.world_body_id == b.world_body_id
				&& a.chunk_id == b.chunk_id
				&& a.grid_coordinate == b.grid_coordinate
				&& a.planet_mesh_generator == b.planet_mesh_generator
				&& a.terrain_node_id == b.terrain_node_id
				&& a.terrain_node_generation_id == b.terrain_node_generation_id
				&& a.surface_only == b.surface_only;
		}

		bool requestHasCompletedResult(const ChunkMeshGenerationRequest& request,
			const DynamicArray<GeneratedChunkMesh>& completed)
		{
			for(const GeneratedChunkMesh& result : completed)
				if(result.generation_id == request.generation_id
					&& result.source == request.source
					&& result.source_revision == request.source_revision
					&& result.planet_generation_id == request.planet_generation_id
					&& result.terrain_revision == request.terrain_revision
					&& result.world_body_id == request.world_body_id
					&& result.chunk_id == request.chunk_id
					&& result.grid_coordinate == request.grid_coordinate
					&& result.planet_mesh_generator == request.planet_mesh_generator
					&& result.terrain_node_id == request.terrain_node_id
					&& result.terrain_node_generation_id == request.terrain_node_generation_id
					&& result.surface_only == request.surface_only) return true;
			return false;
		}

		GeneratedChunkMesh generateChunkMesh(World& world, const ChunkMeshGenerationRequest& request)
		{
			switch(request.source)
			{
				case ChunkMeshSource::planet_terrain_node:
					return generatePlanetTerrainNodeMesh(world, request);
				case ChunkMeshSource::build_parts:
					return generateBuildPartChunkMesh(world, request);
			}
			danikk_engine::formatFatalError("Unsupported chunk mesh source %", static_cast<uint32>(request.source));
			return {};
		}
	}

	size_t Worker::findActiveRequest(const ChunkMeshGenerationRequest& request) const
	{
		for(size_t index = 0; index < active_requests.size(); ++index)
			if(sameRequestKey(active_requests[index].request, request)) return index;
		return active_requests.size();
	}

	bool Worker::isActive(const ChunkMeshGenerationRequest& request) const
	{
		return findActiveRequest(request) < active_requests.size();
	}

	void Worker::removeActiveRequest(size_t index)
	{
		for(size_t shift = index + 1; shift < active_requests.size(); ++shift)
			active_requests[shift - 1] = static_cast<ActiveRequest&&>(active_requests[shift]);
		active_requests.resize(active_requests.size() - 1);
	}

	void Worker::run()
	{
		while(true)
		{
			ChunkMeshGenerationRequest request;
			{
				std::unique_lock<std::mutex> lock(mutex);
				request_available.wait(lock, [this] { return stopping || !requests.empty(); });
				if(stopping) return;
				request = static_cast<ChunkMeshGenerationRequest&&>(requests[0]);
				for(size_t index = 1; index < requests.size(); ++index)
					requests[index - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[index]);
				requests.resize(requests.size() - 1);
				active_requests.push(ActiveRequest{request, false});
			}

			const auto generation_start_time = std::chrono::steady_clock::now();
			GeneratedChunkMesh result = generateChunkMesh(world, request);
			const uint64 generation_nanoseconds = static_cast<uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - generation_start_time).count());
			bool report_generation_sample = false;
			float average_generation_milliseconds = 0.0f;
			float minimum_generation_milliseconds = 0.0f;
			float maximum_generation_milliseconds = 0.0f;
			{
				std::unique_lock<std::mutex> lock(mutex);
				if(generation_sample_count == 0 || generation_nanoseconds < generation_sample_minimum_nanoseconds)
					generation_sample_minimum_nanoseconds = generation_nanoseconds;
				if(generation_sample_count == 0 || generation_nanoseconds > generation_sample_maximum_nanoseconds)
					generation_sample_maximum_nanoseconds = generation_nanoseconds;
				generation_sample_total_nanoseconds += generation_nanoseconds;
				generation_sample_count++;
				if(generation_sample_count == 50)
				{
					constexpr float nanoseconds_per_millisecond = 1000000.0f;
					average_generation_milliseconds = static_cast<float>(generation_sample_total_nanoseconds)
						/ 50.0f / nanoseconds_per_millisecond;
					this->average_generation_milliseconds.store(
						average_generation_milliseconds, std::memory_order_relaxed);
					minimum_generation_milliseconds = static_cast<float>(generation_sample_minimum_nanoseconds)
						/ nanoseconds_per_millisecond;
					maximum_generation_milliseconds = static_cast<float>(generation_sample_maximum_nanoseconds)
						/ nanoseconds_per_millisecond;
					generation_sample_count = 0;
					generation_sample_total_nanoseconds = 0;
					generation_sample_minimum_nanoseconds = 0;
					generation_sample_maximum_nanoseconds = 0;
					report_generation_sample = true;
				}
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
			if constexpr(log_chunk_mesh_generation_statistics)
			{
				if(report_generation_sample)
					danikk_framework::formatLogInfo(
						"Chunk mesh generation (last 50): avg % ms, min % ms, max % ms",
						average_generation_milliseconds,
						minimum_generation_milliseconds,
						maximum_generation_milliseconds);
			}
		}
	}

	Worker::~Worker() { stop(); }

	void Worker::start()
	{
		std::lock_guard<std::mutex> guard(mutex);
		if(!workers.empty()) return;
		average_generation_milliseconds.store(0.0f, std::memory_order_relaxed);
		for(uint8 index = 0; index < static_cast<uint8>(BuildPartShape::count); ++index)
			getBuildPartGeometry(static_cast<BuildPartShape>(index));
		stopping = false;
		const size_t available_threads = std::thread::hardware_concurrency();
		const size_t worker_count = available_threads <= 1
			? 1 : std::min(available_threads - 1, max_worker_count);
		for(size_t index = 0; index < worker_count; ++index)
			workers.pushMove(std::thread(&Worker::run, this));
	}

	bool Worker::getAverageGenerationTimeMilliseconds(float& average_time) const
	{
		average_time = average_generation_milliseconds.load(std::memory_order_relaxed);
		return average_time > 0.0f;
	}

	void Worker::stop()
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

	void Worker::update(uint64 generation_id, const DynamicArray<ChunkMeshGenerationRequest>& new_requests)
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			if(current_generation_id != generation_id)
			{
				current_generation_id = generation_id;
				completed.clear();
				result_space_available.notify_all();
			}

			// Camera movement replaces queued work, but an in-flight mesh can still be
			// useful as a cached parent or child when the camera returns to that region.
			// Explicit planet cancellation and lifecycle changes still discard invalid work.
			result_space_available.notify_all();
			if constexpr(log_planet_mesh_queue_diagnostics)
			{
				static uint32 report_timestamp_ms = 0;
				const uint32 now = danikk_engine::getMsTimestamp();
				if(report_timestamp_ms == 0) report_timestamp_ms = now;
				if(now - report_timestamp_ms >= 1000)
				{
					size_t queued_planet = 0;
					size_t queued_fine = 0;
					size_t desired_planet = 0;
					size_t desired_fine = 0;
					size_t active_planet = 0;
					size_t active_fine = 0;
					size_t active_not_requested = 0;
					size_t completed_planet = 0;
					for(const ChunkMeshGenerationRequest& request : new_requests)
					{
						if(request.source != ChunkMeshSource::planet_terrain_node) continue;
						desired_planet++;
						if(request.chunk_width <= 128.0) desired_fine++;
					}
					for(const ChunkMeshGenerationRequest& request : requests)
					{
						if(request.source != ChunkMeshSource::planet_terrain_node) continue;
						queued_planet++;
						if(request.chunk_width <= 128.0) queued_fine++;
					}
					for(const ActiveRequest& active : active_requests)
					{
						if(active.request.source != ChunkMeshSource::planet_terrain_node) continue;
						active_planet++;
						if(active.request.chunk_width <= 128.0) active_fine++;
						bool requested = false;
						for(const ChunkMeshGenerationRequest& request : new_requests)
							if(sameRequestKey(active.request, request))
							{
								requested = true;
								break;
							}
						if(!requested) active_not_requested++;
					}
					for(const GeneratedChunkMesh& result : completed)
						if(result.source == ChunkMeshSource::planet_terrain_node) completed_planet++;
					danikk_framework::formatLogInfo(
						"Planet mesh queue: desired=% fine_desired=% queued=% fine_queued=% active=% fine_active=% active_not_desired=% completed=%",
						desired_planet, desired_fine, queued_planet, queued_fine,
						active_planet, active_fine, active_not_requested, completed_planet);
					report_timestamp_ms = now;
				}
			}

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

	bool Worker::tryTake(GeneratedChunkMesh& result)
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

	void Worker::cancel(WorldBodyId world_body_id, uint64 planet_generation_id)
	{
		std::lock_guard<std::mutex> guard(mutex);
		for(size_t index = 0; index < requests.size();)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if(request.source != ChunkMeshSource::planet_terrain_node
				|| request.world_body_id != world_body_id
				|| request.planet_generation_id != planet_generation_id)
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
			if(result.source != ChunkMeshSource::planet_terrain_node
				|| result.world_body_id != world_body_id
				|| result.planet_generation_id != planet_generation_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < completed.size(); ++shift)
				completed[shift - 1] = static_cast<GeneratedChunkMesh&&>(completed[shift]);
			completed.resize(completed.size() - 1);
		}
		for(ActiveRequest& active : active_requests)
			if(active.request.source == ChunkMeshSource::planet_terrain_node
				&& active.request.world_body_id == world_body_id
				&& active.request.planet_generation_id == planet_generation_id)
				active.cancelled = true;
		request_available.notify_all();
		result_space_available.notify_all();
	}
}
