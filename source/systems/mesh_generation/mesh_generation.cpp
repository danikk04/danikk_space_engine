#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/world.h>
#include <danikk_framework/log.h>
#include <algorithm>
#include <chrono>
#include <exception>
#include <limits>
#include <thread>

#include <danikk_engine/danikk_engine.h>
#include <danikk_space_engine/systems/planet/data.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/geometry/build_part.h>

#include <danikk_space_engine/systems/mesh_generation/internal.h>

#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/systems/planet/system.h>

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
			thread_pool.initialize(worker_count);
			jobs.resize(worker_count * 2);
		}
		try
		{
			for(size_t index = 0; index < thread_pool.workerCount(); ++index)
				thread_pool.submit([this] { runMeshGenerationTask(); });
		}
		catch(...)
		{
			{
				std::lock_guard<std::mutex> guard(task_mutex);
				stopping_tasks = true;
			}
			job_available.notify_all();
			thread_pool.shutdown();
			{
				std::lock_guard<std::mutex> guard(task_mutex);
				jobs.clear();
				stopping_tasks = false;
			}
			throw;
		}
		initialized = true;
		generation_id++;
		if(generation_id == 0) generation_id++;
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			active_generation_id = generation_id;
		}
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
			for(MeshGenerationJob& job : jobs)
			{
				if(job.state == MeshGenerationJobState::processing) job.cancelled = true;
				else job = MeshGenerationJob();
			}
		}
		job_available.notify_all();
		thread_pool.shutdown();
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			jobs.clear();
			stopping_tasks = false;
		}
		initialized = false;
		generation_id++;
	}

	/** @copydoc MeshGenerationSystem::cancelPlanetJobs */
	void MeshGenerationSystem::cancelPlanetJobs(PlanetId planet_id,
		uint64 planet_generation_id)
	{
		DSE_WATCH();
		cancelJobsForPlanet(planet_id, planet_generation_id);
	}

	void MeshGenerationSystem::invalidateBuildObjectMesh(BuildObjectId build_object_id)
	{
		DSE_WATCH();
		if(!world.getBuildObjectSystem().isBuildObjectActive(build_object_id)) return;
		BuildObject& object = world.getBuildObjectSystem().getBuildObject(build_object_id);
		object.detail_mesh_dirty = true;
		object.detail_mesh_queued = false;
		if(++object.detail_mesh_revision == 0) ++object.detail_mesh_revision;
	}

	bool MeshGenerationSystem::findNextBuildPartMeshJob(
		MeshGenerationJob& job, double& best_distance_squared) const
	{
		const bool has_camera = world.getCameraSystem().isActive();
		const glm::dvec3 camera_position = has_camera
			? world.getCameraSystem().getWorldPosition() : glm::dvec3(0.0);
		best_distance_squared = std::numeric_limits<double>::infinity();
		bool found = false;
		for(WorldBodyId world_body_id : world.getWorldBodySystem().getUsedWorldBodyIds())
		{
			const BuildObjectId object_id = world.getBuildObjectSystem().findBuildObjectId(world_body_id);
			if(object_id == static_cast<BuildObjectId>(-1)) continue;
			const BuildObject& object = world.getBuildObjectSystem().getBuildObject(object_id);
			if(!object.detail_mesh_dirty || object.detail_mesh_queued) continue;
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(world_body_id);
			const glm::dvec3 offset = body.position - camera_position;
			const double distance_squared = has_camera
				? glm::dot(offset, offset) : std::numeric_limits<double>::infinity();
			if(found && distance_squared >= best_distance_squared) continue;
			job = MeshGenerationJob();
			job.source = MeshGenerationSource::build_parts;
			job.source_revision = object.detail_mesh_revision;
			job.source_generation_id = object.generation_id;
			job.world_body_id = world_body_id;
			job.build_object_id = object_id;
			best_distance_squared = distance_squared;
			found = true;
		}
		return found;
	}

	bool MeshGenerationSystem::isMeshJobWanted(const MeshGenerationJob& job) const
	{
		if(job.source == MeshGenerationSource::planet_terrain_node)
			return world.getPlanetSystem().isMeshGenerationJobWanted(job);
		if(job.source != MeshGenerationSource::build_parts
			|| !world.getWorldBodySystem().isWorldBodyActive(job.world_body_id)
			|| !world.getBuildObjectSystem().isBuildObjectActive(job.build_object_id)) return false;
		const BuildObject& object = world.getBuildObjectSystem().getBuildObject(job.build_object_id);
		return object.world_body_id == job.world_body_id
			&& object.generation_id == job.source_generation_id
			&& object.detail_mesh_queued && object.detail_mesh_revision == job.source_revision;
	}

	void MeshGenerationSystem::setMeshJobPending(const MeshGenerationJob& job, bool pending)
	{
		if(job.source == MeshGenerationSource::planet_terrain_node)
		{
			world.getPlanetSystem().setMeshGenerationJobPending(job, pending);
			return;
		}
		if(job.source != MeshGenerationSource::build_parts
			|| !world.getWorldBodySystem().isWorldBodyActive(job.world_body_id)
			|| !world.getBuildObjectSystem().isBuildObjectActive(job.build_object_id)) return;
		BuildObject& object = world.getBuildObjectSystem().getBuildObject(job.build_object_id);
		if(object.world_body_id != job.world_body_id
			|| object.generation_id != job.source_generation_id
			|| object.detail_mesh_revision != job.source_revision) return;
		object.detail_mesh_queued = pending;
		if(!pending) object.detail_mesh_dirty = true;
	}

	void MeshGenerationSystem::markMeshJobScheduled(const MeshGenerationJob& job)
	{
		if(job.source == MeshGenerationSource::planet_terrain_node)
		{
			world.getPlanetSystem().setMeshGenerationJobPending(job, true);
			return;
		}
		if(job.source != MeshGenerationSource::build_parts
			|| !world.getWorldBodySystem().isWorldBodyActive(job.world_body_id)
			|| !world.getBuildObjectSystem().isBuildObjectActive(job.build_object_id)) return;
		BuildObject& object = world.getBuildObjectSystem().getBuildObject(job.build_object_id);
		if(object.world_body_id != job.world_body_id
			|| object.generation_id != job.source_generation_id
			|| object.detail_mesh_revision != job.source_revision || !object.detail_mesh_dirty) return;
		if(object.detail_mesh.mesh != nullptr)
			world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
				MeshDeletionReason::detail_mesh_rebuild);
		destroyMeshRenderData(object.detail_mesh);
		object.detail_mesh_dirty = false;
		object.detail_mesh_queued = true;
	}

	/** @copydoc MeshGenerationSystem::frame */
	void MeshGenerationSystem::frame()
	{
		DSE_WATCH();
		MeshGenerationJob generated;
		if(tryTakeCompletedJob(generated) && generated.generation_id == generation_id)
		{
			if(generated.source == MeshGenerationSource::planet_terrain_node)
				world.getPlanetSystem().acceptGeneratedTerrainNodeMesh(generated);
			else if(generated.source == MeshGenerationSource::build_parts
				&& world.getWorldBodySystem().isWorldBodyActive(generated.world_body_id)
				&& world.getBuildObjectSystem().isBuildObjectActive(generated.build_object_id))
			{
				BuildObject& object = world.getBuildObjectSystem().getBuildObject(generated.build_object_id);
				if(object.world_body_id == generated.world_body_id
					&& object.generation_id == generated.source_generation_id)
				{
					if(object.detail_mesh_revision == generated.source_revision)
					{
						createMeshRenderData(object.detail_mesh,
							generated.point_positions.data(), generated.point_positions.size(),
							generated.triangle_point_indices.data(), generated.triangle_point_indices.size(),
							generated.triangle_material_ids.data(), generated.triangle_material_ids.size());
						object.detail_mesh_position = generated.mesh_position;
						object.detail_mesh_queued = false;
						object.detail_mesh_bounds_valid = false;
						if(generated.point_positions.size() > 0)
						{
							vec3 minimum = generated.point_positions[0];
							vec3 maximum = generated.point_positions[0];
							for(const vec3& position : generated.point_positions)
							{
								minimum = glm::min(minimum, position);
								maximum = glm::max(maximum, position);
							}
							object.detail_mesh_bounds_center = (minimum + maximum) * 0.5f;
							object.detail_mesh_bounds_radius = 0.0f;
							for(const vec3& position : generated.point_positions)
								object.detail_mesh_bounds_radius = glm::max(object.detail_mesh_bounds_radius,
									glm::length(position - object.detail_mesh_bounds_center));
							object.detail_mesh_bounds_valid = true;
						}
					}
				}
			}
		}
		updateMeshJobs();
	}
}
namespace danikk_space_engine::mesh_generation_internal
{
	void generateMesh(World& world, MeshGenerationJob& job)
	{
		switch(job.source)
		{
			case MeshGenerationSource::planet_terrain_node:
				generatePlanetTerrainNodeMesh(world, job);
				return;
			case MeshGenerationSource::build_parts:
				generateBuildPartMesh(world, job);
				return;
		}
		danikk_engine::formatFatalError("Unsupported mesh source %", static_cast<uint32>(job.source));
	}
}

namespace danikk_space_engine
{
	using namespace mesh_generation_internal;

	void MeshGenerationSystem::runMeshGenerationTask()
	{
		while(true)
		{
			size_t job_index = jobs.size();
			{
				std::unique_lock<std::mutex> lock(task_mutex);
				job_available.wait(lock, [this]
				{
					if(stopping_tasks) return true;
					for(const MeshGenerationJob& job : jobs)
						if(job.state == MeshGenerationJobState::queued) return true;
					return false;
				});
				if(stopping_tasks) return;
				for(size_t index = 0; index < jobs.size(); ++index)
					if(jobs[index].state == MeshGenerationJobState::queued)
					{
						job_index = index;
						jobs[index].state = MeshGenerationJobState::processing;
						break;
					}
			}
			if(job_index < jobs.size()) runMeshGenerationJob(job_index);
		}
	}

	void MeshGenerationSystem::runMeshGenerationJob(size_t job_index)
	{
		const auto generation_start_time = std::chrono::steady_clock::now();
		MeshGenerationJob& job = jobs[job_index];
		bool generation_succeeded = false;
		try
		{
			mesh_generation_internal::generateMesh(world, job);
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
		if(job.source == MeshGenerationSource::planet_terrain_node)
			world.getTelemetrySystem().mesh_generation_telemetry.recordPlanetMeshGeneration(
				job.terrain_node_depth,
				generation_succeeded ? job.triangle_point_indices.size() / 3 : 0,
				generation_nanoseconds);

		std::lock_guard<std::mutex> lock(task_mutex);
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
		if(stopping_tasks || job.cancelled || job.generation_id != active_generation_id)
		{
			job = MeshGenerationJob();
			return;
		}
		job.cancelled = false;
		job.state = MeshGenerationJobState::completed;
	}

	void MeshGenerationSystem::updateMeshJobs()
	{
		DSE_WATCH();
		if(!initialized) return;
		PlanetMeshQueueTelemetry snapshot;
		size_t empty_job_indices[8] = {};
		size_t empty_job_count = 0;
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			for(MeshGenerationJob& job : jobs)
			{
				if(job.state != MeshGenerationJobState::queued || isMeshJobWanted(job)) continue;
				setMeshJobPending(job, false);
				job = MeshGenerationJob();
			}
			for(size_t index = 0; index < jobs.size(); ++index)
			{
				if(jobs[index].state == MeshGenerationJobState::empty)
					empty_job_indices[empty_job_count++] = index;
			}
		}

		bool jobs_added = false;
		for(size_t empty_index = 0; empty_index < empty_job_count; ++empty_index)
		{
			MeshGenerationJob planet_job;
			MeshGenerationJob build_part_job;
			double planet_distance_squared = std::numeric_limits<double>::infinity();
			double build_part_distance_squared = std::numeric_limits<double>::infinity();
			const bool has_planet_job = world.getPlanetSystem().findNextMeshGenerationJob(
				planet_job, planet_distance_squared);
			const bool has_build_part_job = findNextBuildPartMeshJob(
				build_part_job, build_part_distance_squared);
			if(!has_planet_job && !has_build_part_job) break;

			MeshGenerationJob& selected_job = has_planet_job
				&& (!has_build_part_job || planet_distance_squared <= build_part_distance_squared)
				? planet_job : build_part_job;
			selected_job.generation_id = generation_id;
			markMeshJobScheduled(selected_job);
			selected_job.state = MeshGenerationJobState::queued;
			{
				std::lock_guard<std::mutex> guard(task_mutex);
				jobs[empty_job_indices[empty_index]] = static_cast<MeshGenerationJob&&>(selected_job);
			}
			jobs_added = true;
		}

		{
			std::lock_guard<std::mutex> guard(task_mutex);
			for(const MeshGenerationJob& job : jobs)
			{
				if(job.state == MeshGenerationJobState::empty
					|| job.source != MeshGenerationSource::planet_terrain_node) continue;
				if(job.state == MeshGenerationJobState::queued)
				{
					snapshot.queued++;
					if(job.node_width <= 128.0) snapshot.fine_queued++;
				}
				else if(job.state == MeshGenerationJobState::processing)
				{
					snapshot.active++;
					if(job.node_width <= 128.0) snapshot.fine_active++;
				}
				else if(job.state == MeshGenerationJobState::completed) snapshot.completed++;
			}
		}
		if(jobs_added) job_available.notify_all();
		world.getTelemetrySystem().mesh_generation_telemetry.setPlanetMeshQueueSnapshot(snapshot);
	}

	bool MeshGenerationSystem::tryTakeCompletedJob(MeshGenerationJob& completed_job)
	{
		std::lock_guard<std::mutex> guard(task_mutex);
		for(size_t index = 0; index < jobs.size(); ++index)
		{
			if(jobs[index].state != MeshGenerationJobState::completed) continue;
			if(jobs[index].generation_id != active_generation_id)
			{
				jobs[index] = MeshGenerationJob();
				continue;
			}
			completed_job = static_cast<MeshGenerationJob&&>(jobs[index]);
			jobs[index] = MeshGenerationJob();
			return true;
		}
		return false;
	}

	void MeshGenerationSystem::cancelJobsForPlanet(PlanetId planet_id,
		uint64 planet_generation_id)
	{
		std::lock_guard<std::mutex> guard(task_mutex);
		for(MeshGenerationJob& job : jobs)
		{
			if(job.state == MeshGenerationJobState::empty
				|| job.source != MeshGenerationSource::planet_terrain_node
				|| job.planet_id != planet_id
				|| job.planet_generation_id != planet_generation_id) continue;
			if(job.state == MeshGenerationJobState::processing) job.cancelled = true;
			else job = MeshGenerationJob();
		}
		job_available.notify_all();
	}
}
