#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_framework/log.h>
#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/geometry/build_part.h>

#include "internal.h"

namespace danikk_space_engine
{
	MeshGenerationSystem::MeshGenerationSystem(World& world)
		: world(world) {}

	MeshGenerationSystem::~MeshGenerationSystem() { shutdown(); }

	bool MeshGenerationSystem::getAverageGenerationTimeMilliseconds(float& average_time) const
	{
		average_time = average_generation_milliseconds.load(std::memory_order_relaxed);
		return average_time > 0.0f;
	}

	/** @copydoc MeshGenerationSystem::init */
	void MeshGenerationSystem::init()
	{
		DSE_WATCH();
		if(initialized) return;
		danikk_framework::formatLogInfo("Init MeshGenerationSystem");
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			stopping_tasks = false;
			generation_sample_count = 0;
			generation_sample_total_nanoseconds = 0;
			average_generation_milliseconds.store(0.0f, std::memory_order_relaxed);
			const size_t available_threads = std::thread::hardware_concurrency();
			const size_t worker_count = available_threads <= 1 ? 1 : std::min(available_threads - 1, size_t(4));
			thread_pool = std::make_unique<danikk_framework::ThreadPool>(worker_count);
			task_request_slots.resize(worker_count);
		}
		try
		{
			for(size_t index = 0; index < task_request_slots.size(); ++index)
				thread_pool->submit([this, index] { runMeshGenerationTask(index); });
		}
		catch(...)
		{
			{
				std::lock_guard<std::mutex> guard(task_mutex);
				stopping_tasks = true;
			}
			request_available.notify_all();
			result_space_available.notify_all();
			thread_pool->shutdown();
			thread_pool.reset();
			{
				std::lock_guard<std::mutex> guard(task_mutex);
				task_request_slots.clear();
				stopping_tasks = false;
			}
			throw;
		}
		initialized = true;
		generation_id++;
		if(generation_id == 0) generation_id++;
		requests.clear();
		updateMeshGenerationRequests(generation_id, requests);
	}

	/** @copydoc MeshGenerationSystem::shutdown */
	void MeshGenerationSystem::shutdown()
	{
		DSE_WATCH();
		if(!initialized) return;
		danikk_framework::formatLogInfo("Shutdown MeshGenerationSystem");
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			stopping_tasks = true;
			queued_requests.clear();
			completed_meshes.clear();
			for(TaskRequestSlot& slot : task_request_slots)
				if(slot.has_request) slot.cancelled = true;
		}
		request_available.notify_all();
		result_space_available.notify_all();
		if(thread_pool != nullptr) thread_pool->shutdown();
		thread_pool.reset();
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			task_request_slots.clear();
			stopping_tasks = false;
		}
		initialized = false;
		generation_id++;
		requests.clear();
	}

	/** @copydoc MeshGenerationSystem::beginRequestFrame */
	void MeshGenerationSystem::beginRequestFrame()
	{
		DSE_WATCH();
		requests.clear();
	}

	/** @copydoc MeshGenerationSystem::appendPlanetRequests */
	void MeshGenerationSystem::appendPlanetRequests(
		const DynamicArray<MeshGenerationRequest>& planet_requests)
	{
		DSE_WATCH();
		for(const MeshGenerationRequest& request : planet_requests)
		{
			MeshGenerationRequest queued = request;
			queued.generation_id = generation_id;
			requests.pushMove(queued);
		}
	}

	/** @copydoc MeshGenerationSystem::cancelPlanetRequests */
	void MeshGenerationSystem::cancelPlanetRequests(PlanetId planet_id,
		uint64 planet_generation_id)
	{
		DSE_WATCH();
		cancelQueuedPlanetRequests(planet_id, planet_generation_id);
		for(size_t index = 0; index < requests.size();)
		{
			const MeshGenerationRequest& request = requests[index];
			if(request.source != MeshGenerationSource::planet_terrain_node
				|| request.planet_id != planet_id
				|| request.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<MeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
		}
	}

	void MeshGenerationSystem::invalidateBodyDetailMesh(WorldBodyId world_body_id)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(world_body_id)) return;
		WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		body.detail_mesh_dirty = true;
		body.detail_mesh_queued = false;
		if(++body.detail_mesh_revision == 0) ++body.detail_mesh_revision;
	}

	/** @copydoc MeshGenerationSystem::collectBuildPartRequests */
	void MeshGenerationSystem::collectBuildPartRequests()
	{
		DSE_WATCH();
		for(WorldBodyId world_body_id : world.world_body_data.getUsedWorldBodyIds())
		{
			WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
			if(!body.detail_mesh_dirty && !body.detail_mesh_queued) continue;
			if(body.detail_mesh_dirty)
			{
				destroyMeshRenderData(body.detail_mesh);
				body.detail_mesh_dirty = false;
				body.detail_mesh_queued = true;
			}

			MeshGenerationRequest request;
			request.source = MeshGenerationSource::build_parts;
			request.generation_id = generation_id;
			request.source_revision = body.detail_mesh_revision;
			request.world_body_id = world_body_id;
			requests.pushMove(request);
		}
	}

	/** @copydoc MeshGenerationSystem::frame */
	void MeshGenerationSystem::frame()
	{
		DSE_WATCH();
		collectBuildPartRequests();
		updateMeshGenerationRequests(generation_id, world.planets.buildMeshGenerationRequestBuffer(
			requests, thread_pool != nullptr ? thread_pool->workerCount() : 0));

		GeneratedMesh generated;
		if(!tryTakeGeneratedMesh(generated) || generated.generation_id != generation_id) return;
		if(generated.source == MeshGenerationSource::planet_terrain_node)
		{
			world.planets.acceptGeneratedSurfaceNode(generated);
		}
		else if(generated.source == MeshGenerationSource::build_parts)
		{
			if(!world.world_body_data.isWorldBodyActive(generated.world_body_id)) return;
			WorldBody& body = world.world_body_data.getWorldBody(generated.world_body_id);
			if(body.detail_mesh_revision != generated.source_revision) return;
			createMeshRenderData(body.detail_mesh,
				generated.point_positions.data(), generated.point_positions.size(),
				generated.triangle_point_indices.data(), generated.triangle_point_indices.size(),
				generated.triangle_material_ids.data(), generated.triangle_material_ids.size());
			body.detail_mesh_position = generated.mesh_position;
			body.detail_mesh_queued = false;
			body.detail_mesh_bounds_valid = false;
			if(generated.point_positions.size() > 0)
			{
				vec3 minimum = generated.point_positions[0];
				vec3 maximum = generated.point_positions[0];
				for(const vec3& position : generated.point_positions)
				{
					minimum = glm::min(minimum, position);
					maximum = glm::max(maximum, position);
				}
				body.detail_mesh_bounds_center = (minimum + maximum) * 0.5f;
				body.detail_mesh_bounds_radius = 0.0f;
				for(const vec3& position : generated.point_positions)
					body.detail_mesh_bounds_radius = glm::max(body.detail_mesh_bounds_radius,
						glm::length(position - body.detail_mesh_bounds_center));
				body.detail_mesh_bounds_valid = true;
			}
		}
	}
}
namespace danikk_space_engine::mesh_generation_internal
{
	namespace
	{
		constexpr size_t max_completed_meshes = 2;

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
}

namespace danikk_space_engine
{
	using namespace mesh_generation_internal;

	size_t MeshGenerationSystem::findActiveRequest(const MeshGenerationRequest& request) const
	{
		for(size_t index = 0; index < task_request_slots.size(); ++index)
			if(task_request_slots[index].has_request
				&& sameRequestKey(task_request_slots[index].request, request)) return index;
		return task_request_slots.size();
	}

	bool MeshGenerationSystem::isActiveRequest(const MeshGenerationRequest& request) const
	{
		return findActiveRequest(request) < task_request_slots.size();
	}

	void MeshGenerationSystem::clearTaskRequestSlot(size_t task_index)
	{
		task_request_slots[task_index].has_request = false;
		task_request_slots[task_index].cancelled = false;
	}

	void MeshGenerationSystem::runMeshGenerationTask(size_t task_index)
	{
		while(true)
		{
			MeshGenerationRequest request;
			{
				std::unique_lock<std::mutex> lock(task_mutex);
				request_available.wait(lock, [this] { return stopping_tasks || !queued_requests.empty(); });
				if(stopping_tasks) return;
				request = queued_requests[0];
				for(size_t index = 1; index < queued_requests.size(); ++index)
					queued_requests[index - 1] = static_cast<MeshGenerationRequest&&>(queued_requests[index]);
				queued_requests.resize(queued_requests.size() - 1);
				if(request.generation_id != active_generation_id || isActiveRequest(request)
					|| requestHasCompletedResult(request, completed_meshes)) continue;
				task_request_slots[task_index].request = request;
				task_request_slots[task_index].has_request = true;
				task_request_slots[task_index].cancelled = false;
			}
			generateMeshRequest(task_index, request);
		}
	}

	void MeshGenerationSystem::generateMeshRequest(size_t task_index,
		const MeshGenerationRequest& request)
	{
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
			result = mesh_generation_internal::generateMesh(world, request);
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
				mesh_generation_internal::getPlanetTerrainNodeLevel(request.node_width),
				generation_succeeded ? result.triangle_point_indices.size() / 3 : 0,
				generation_nanoseconds);

		std::unique_lock<std::mutex> lock(task_mutex);
		generation_sample_total_nanoseconds += generation_nanoseconds;
		generation_sample_count++;
		if(generation_sample_count == 50)
		{
			constexpr float nanoseconds_per_millisecond = 1000000.0f;
			const float average = static_cast<float>(generation_sample_total_nanoseconds)
				/ 50.0f / nanoseconds_per_millisecond;
			average_generation_milliseconds.store(average, std::memory_order_relaxed);
			generation_sample_count = 0;
			generation_sample_total_nanoseconds = 0;
		}
		result_space_available.wait(lock, [this, task_index]
		{
			return stopping_tasks || completed_meshes.size() < max_completed_meshes
				|| !task_request_slots[task_index].has_request || task_request_slots[task_index].cancelled;
		});
		const bool cancelled = task_request_slots[task_index].cancelled;
		clearTaskRequestSlot(task_index);
		if(!stopping_tasks && !cancelled && request.generation_id == active_generation_id)
			completed_meshes.pushMove(result);
	}

	void MeshGenerationSystem::updateMeshGenerationRequests(uint64 new_generation_id,
		const DynamicArray<MeshGenerationRequest>& new_requests)
	{
		PlanetMeshQueueTelemetry snapshot;
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			if(active_generation_id != new_generation_id)
			{
				active_generation_id = new_generation_id;
				completed_meshes.clear();
				result_space_available.notify_all();
			}
			result_space_available.notify_all();

			size_t queue_index = 0;
			bool same_queue = true;
			for(const MeshGenerationRequest& request : new_requests)
			{
				if(request.generation_id != active_generation_id || isActiveRequest(request)
					|| requestHasCompletedResult(request, completed_meshes)) continue;
				if(queue_index >= queued_requests.size()
					|| !sameRequestKey(queued_requests[queue_index], request))
				{
					same_queue = false;
					break;
				}
				queue_index++;
			}
			if(!same_queue || queue_index != queued_requests.size())
			{
				queued_requests.clear();
				for(const MeshGenerationRequest& request : new_requests)
					if(request.generation_id == active_generation_id && !isActiveRequest(request)
						&& !requestHasCompletedResult(request, completed_meshes))
						queued_requests.push(request);
			}

			for(const MeshGenerationRequest& request : new_requests)
			{
				if(request.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.desired++;
				if(request.node_width <= 128.0) snapshot.fine_desired++;
			}
			for(const MeshGenerationRequest& request : queued_requests)
			{
				if(request.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.queued++;
				if(request.node_width <= 128.0) snapshot.fine_queued++;
			}
			for(const TaskRequestSlot& active : task_request_slots)
			{
				if(!active.has_request || active.request.source != MeshGenerationSource::planet_terrain_node) continue;
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
			for(const GeneratedMesh& result : completed_meshes)
				if(result.source == MeshGenerationSource::planet_terrain_node) snapshot.completed++;
		}
		request_available.notify_all();
		telemetry_system.setPlanetMeshQueueSnapshot(snapshot);
	}

	bool MeshGenerationSystem::tryTakeGeneratedMesh(GeneratedMesh& result)
	{
		std::lock_guard<std::mutex> guard(task_mutex);
		while(!completed_meshes.empty())
		{
			GeneratedMesh candidate = static_cast<GeneratedMesh&&>(completed_meshes[0]);
			for(size_t index = 1; index < completed_meshes.size(); ++index)
				completed_meshes[index - 1] = static_cast<GeneratedMesh&&>(completed_meshes[index]);
			completed_meshes.resize(completed_meshes.size() - 1);
			if(candidate.generation_id != active_generation_id) continue;
			result = static_cast<GeneratedMesh&&>(candidate);
			result_space_available.notify_one();
			return true;
		}
		return false;
	}

	void MeshGenerationSystem::cancelQueuedPlanetRequests(PlanetId planet_id,
		uint64 planet_generation_id)
	{
		std::lock_guard<std::mutex> guard(task_mutex);
		for(size_t index = 0; index < queued_requests.size();)
		{
			const MeshGenerationRequest& request = queued_requests[index];
			if(request.source != MeshGenerationSource::planet_terrain_node
				|| request.planet_id != planet_id
				|| request.planet_generation_id != planet_generation_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < queued_requests.size(); ++shift)
				queued_requests[shift - 1] = static_cast<MeshGenerationRequest&&>(queued_requests[shift]);
			queued_requests.resize(queued_requests.size() - 1);
		}
		for(size_t index = 0; index < completed_meshes.size();)
		{
			const GeneratedMesh& result = completed_meshes[index];
			if(result.source != MeshGenerationSource::planet_terrain_node
				|| result.planet_id != planet_id
				|| result.planet_generation_id != planet_generation_id)
			{
				++index;
				continue;
			}
			for(size_t shift = index + 1; shift < completed_meshes.size(); ++shift)
				completed_meshes[shift - 1] = static_cast<GeneratedMesh&&>(completed_meshes[shift]);
			completed_meshes.resize(completed_meshes.size() - 1);
		}
		for(TaskRequestSlot& active : task_request_slots)
			if(active.has_request && active.request.source == MeshGenerationSource::planet_terrain_node
				&& active.request.planet_id == planet_id
				&& active.request.planet_generation_id == planet_generation_id)
				active.cancelled = true;
		request_available.notify_all();
		result_space_available.notify_all();
	}
}
