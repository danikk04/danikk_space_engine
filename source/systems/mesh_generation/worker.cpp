#include <algorithm>
#include <chrono>
#include <exception>

#include <danikk_engine/danikk_engine.h>
#include <danikk_framework/log.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/data.h>

#include "internal.h"

namespace danikk_space_engine::mesh_generation_internal
{
	namespace
	{
		constexpr size_t max_completed_meshes = 2;
		constexpr size_t max_worker_count = 4;

		uint8 getPlanetTerrainNodeLevel(double node_width)
		{
			uint8 level = 0;
			double width = PlanetTerrainTree::initial_root_half_extent * 2.0;
			while(level < 63 && node_width < width)
			{
				width *= 0.5;
				level++;
			}
			return level;
		}

		bool sameRequestKey(const MeshGenerationRequest& a, const MeshGenerationRequest& b)
		{
			return a.generation_id == b.generation_id
				&& a.source == b.source
				&& a.source_revision == b.source_revision
				&& a.planet_generation_id == b.planet_generation_id
				&& a.terrain_revision == b.terrain_revision
				&& a.world_body_id == b.world_body_id
				&& a.planet_id == b.planet_id
				&& a.planet_mesh_generator == b.planet_mesh_generator
				&& a.terrain_node_id == b.terrain_node_id
				&& a.terrain_node_generation_id == b.terrain_node_generation_id
				&& a.surface_only == b.surface_only;
		}

		bool requestHasCompletedResult(const MeshGenerationRequest& request,
			const DynamicArray<GeneratedMesh>& completed)
		{
			for(const GeneratedMesh& result : completed)
				if(result.generation_id == request.generation_id
					&& result.source == request.source
					&& result.source_revision == request.source_revision
					&& result.planet_generation_id == request.planet_generation_id
					&& result.terrain_revision == request.terrain_revision
					&& result.world_body_id == request.world_body_id
					&& result.planet_id == request.planet_id
					&& result.planet_mesh_generator == request.planet_mesh_generator
					&& result.terrain_node_id == request.terrain_node_id
					&& result.terrain_node_generation_id == request.terrain_node_generation_id
					&& result.surface_only == request.surface_only) return true;
			return false;
		}

		GeneratedMesh generateMesh(World& world, const MeshGenerationRequest& request)
		{
			switch(request.source)
			{
				case MeshGenerationSource::planet_terrain_node:
					return generatePlanetTerrainNodeMesh(world, request);
				case MeshGenerationSource::build_parts:
					return generateBuildPartMesh(world, request);
			}
			danikk_engine::formatFatalError("Unsupported mesh source %", static_cast<uint32>(request.source));
			return {};
		}
	}

	size_t Worker::findActiveRequest(const MeshGenerationRequest& request) const
	{
		for(size_t index = 0; index < active_requests.size(); ++index)
			if(sameRequestKey(active_requests[index].request, request)) return index;
		return active_requests.size();
	}

	bool Worker::isActive(const MeshGenerationRequest& request) const
	{
		return findActiveRequest(request) < active_requests.size();
	}

	void Worker::removeActiveRequest(size_t index)
	{
		for(size_t shift = index + 1; shift < active_requests.size(); ++shift)
			active_requests[shift - 1] = static_cast<ActiveRequest&&>(active_requests[shift]);
		active_requests.resize(active_requests.size() - 1);
	}

	void Worker::dispatchPendingRequestsLocked()
	{
		if(stopping || thread_pool == nullptr) return;
		const size_t worker_count = thread_pool->workerCount();
		while(!requests.empty() && active_requests.size() < worker_count)
		{
			const MeshGenerationRequest request = requests[0];
			if(request.generation_id != current_generation_id || isActive(request)
				|| requestHasCompletedResult(request, completed))
			{
				for(size_t index = 1; index < requests.size(); ++index)
					requests[index - 1] = static_cast<MeshGenerationRequest&&>(requests[index]);
				requests.resize(requests.size() - 1);
				continue;
			}

			active_requests.push(ActiveRequest{request, false});
			try
			{
				thread_pool->submit([this, request] { generateRequest(request); });
			}
			catch(...)
			{
				removeActiveRequest(active_requests.size() - 1);
				throw;
			}
			for(size_t index = 1; index < requests.size(); ++index)
				requests[index - 1] = static_cast<MeshGenerationRequest&&>(requests[index]);
			requests.resize(requests.size() - 1);
		}
	}

	void Worker::generateRequest(const MeshGenerationRequest& request)
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			const size_t active_index = findActiveRequest(request);
			if(stopping || active_index >= active_requests.size()
				|| active_requests[active_index].cancelled)
			{
				if(active_index < active_requests.size()) removeActiveRequest(active_index);
				dispatchPendingRequestsLocked();
				return;
			}
		}

		const auto generation_start_time = std::chrono::steady_clock::now();
		GeneratedMesh result;
		result.generation_id = request.generation_id;
		result.source_revision = request.source_revision;
		result.world_body_id = request.world_body_id;
		result.planet_id = request.planet_id;
		result.planet_generation_id = request.planet_generation_id;
		result.terrain_revision = request.terrain_revision;
		result.terrain_node_id = request.terrain_node_id;
		result.terrain_node_generation_id = request.terrain_node_generation_id;
		result.surface_only = request.surface_only;
		result.source = request.source;
		result.planet_mesh_generator = request.planet_mesh_generator;
		result.node_minimum = request.node_minimum;
		bool generation_succeeded = false;
		try
		{
			result = generateMesh(world, request);
			if(request.source == MeshGenerationSource::planet_terrain_node)
				result.planet_id = request.planet_id;
			generation_succeeded = true;
		}
		catch(const std::exception& exception)
		{
			danikk_framework::formatLogError("Mesh generation task failed: %", exception.what());
		}
		catch(...)
		{
			danikk_framework::formatLogError("Mesh generation task failed with an unknown exception");
		}
		const uint64 generation_nanoseconds = static_cast<uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - generation_start_time).count());
		if(request.source == MeshGenerationSource::planet_terrain_node)
			telemetry_system.recordPlanetMeshGeneration(
				getPlanetTerrainNodeLevel(request.node_width),
			generation_succeeded ? result.triangle_point_indices.size() / 3 : 0,
			generation_nanoseconds);
		{
			std::unique_lock<std::mutex> lock(mutex);
			generation_sample_total_nanoseconds += generation_nanoseconds;
			generation_sample_count++;
			if(generation_sample_count == 50)
			{
				constexpr float nanoseconds_per_millisecond = 1000000.0f;
				const float average_generation_milliseconds = static_cast<float>(generation_sample_total_nanoseconds)
					/ 50.0f / nanoseconds_per_millisecond;
				this->average_generation_milliseconds.store(
					average_generation_milliseconds, std::memory_order_relaxed);
				generation_sample_count = 0;
				generation_sample_total_nanoseconds = 0;
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
			if(!stopping && !cancelled
				&& request.generation_id == current_generation_id)
				completed.pushMove(result);
			dispatchPendingRequestsLocked();
		}
	}

	Worker::~Worker() { stop(); }

	void Worker::start()
	{
		std::lock_guard<std::mutex> guard(mutex);
		if(thread_pool != nullptr) return;
		average_generation_milliseconds.store(0.0f, std::memory_order_relaxed);
		generation_sample_count = 0;
		generation_sample_total_nanoseconds = 0;
		for(uint8 index = 0; index < static_cast<uint8>(BuildPartShape::count); ++index)
			getBuildPartGeometry(static_cast<BuildPartShape>(index));
		stopping = false;
		const size_t available_threads = std::thread::hardware_concurrency();
		const size_t worker_count = available_threads <= 1
			? 1 : std::min(available_threads - 1, max_worker_count);
		thread_pool = std::make_unique<danikk_framework::ThreadPool>(worker_count);
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
			if(thread_pool == nullptr) return;
			stopping = true;
			requests.clear();
			completed.clear();
			for(ActiveRequest& active : active_requests) active.cancelled = true;
		}
		result_space_available.notify_all();
		thread_pool->shutdown();
		thread_pool.reset();
		{
			std::lock_guard<std::mutex> guard(mutex);
			active_requests.clear();
			stopping = false;
		}
	}

	void Worker::update(uint64 generation_id, const DynamicArray<MeshGenerationRequest>& new_requests)
	{
		PlanetMeshQueueTelemetry snapshot;
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

			size_t queue_index = 0;
			bool same_queue = true;
			for(const MeshGenerationRequest& request : new_requests)
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
			if(!same_queue || queue_index != requests.size())
			{
				requests.clear();
				for(const MeshGenerationRequest& request : new_requests)
				{
					if(request.generation_id != current_generation_id
						|| isActive(request)
						|| requestHasCompletedResult(request, completed)) continue;
					requests.push(request);
				}
			}
			dispatchPendingRequestsLocked();

			for(const MeshGenerationRequest& request : new_requests)
			{
				if(request.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.desired++;
				if(request.node_width <= 128.0) snapshot.fine_desired++;
			}
			for(const MeshGenerationRequest& request : requests)
			{
				if(request.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.queued++;
				if(request.node_width <= 128.0) snapshot.fine_queued++;
			}
			for(const ActiveRequest& active : active_requests)
			{
				if(active.request.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.active++;
				if(active.request.node_width <= 128.0) snapshot.fine_active++;
				bool requested = false;
				for(const MeshGenerationRequest& request : new_requests)
					if(sameRequestKey(active.request, request))
					{
						requested = true;
						break;
					}
				if(!requested) snapshot.active_not_desired++;
			}
			for(const GeneratedMesh& result : completed)
				if(result.source == MeshGenerationSource::planet_terrain_node) snapshot.completed++;
		}
		telemetry_system.setPlanetMeshQueueSnapshot(snapshot);
	}

	bool Worker::tryTake(GeneratedMesh& result)
	{
		std::lock_guard<std::mutex> guard(mutex);
		while(!completed.empty())
		{
			GeneratedMesh candidate = static_cast<GeneratedMesh&&>(completed[0]);
			for(size_t index = 1; index < completed.size(); ++index)
				completed[index - 1] = static_cast<GeneratedMesh&&>(completed[index]);
			completed.resize(completed.size() - 1);
			if(candidate.generation_id != current_generation_id) continue;
			result = static_cast<GeneratedMesh&&>(candidate);
			result_space_available.notify_one();
			return true;
		}
		return false;
	}

	void Worker::cancel(PlanetId planet_id, uint64 planet_generation_id)
	{
		std::lock_guard<std::mutex> guard(mutex);
		for(size_t index = 0; index < requests.size();)
		{
			const MeshGenerationRequest& request = requests[index];
			if(request.source != MeshGenerationSource::planet_terrain_node
				|| request.planet_id != planet_id
				|| request.planet_generation_id != planet_generation_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); ++shift)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
		}
		for(size_t index = 0; index < completed.size();)
		{
			const GeneratedMesh& result = completed[index];
			if(result.source != MeshGenerationSource::planet_terrain_node
				|| result.planet_id != planet_id
				|| result.planet_generation_id != planet_generation_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < completed.size(); ++shift)
				completed[shift - 1] = static_cast<GeneratedMesh&&>(completed[shift]);
			completed.resize(completed.size() - 1);
		}
		for(ActiveRequest& active : active_requests)
			if(active.request.source == MeshGenerationSource::planet_terrain_node
				&& active.request.planet_id == planet_id
				&& active.request.planet_generation_id == planet_generation_id)
				active.cancelled = true;
		result_space_available.notify_all();
	}
}
