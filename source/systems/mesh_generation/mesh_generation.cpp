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

#include <danikk_space_engine/world_body.h>
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
		job_candidates.clear();
		updateMeshGenerationJobs(generation_id, job_candidates);
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
		job_candidates.clear();
	}

	/** @copydoc MeshGenerationSystem::beginJobCandidateFrame */
	void MeshGenerationSystem::beginJobCandidateFrame()
	{
		DSE_WATCH();
		job_candidates.clear();
	}

	/** @copydoc MeshGenerationSystem::appendPlanetJobCandidates */
	void MeshGenerationSystem::appendPlanetJobCandidates(
		const DynamicArray<MeshGenerationJob>& planet_job_candidates)
	{
		DSE_WATCH();
		for(const MeshGenerationJob& candidate : planet_job_candidates)
		{
			MeshGenerationJob queued = candidate;
			queued.generation_id = generation_id;
			job_candidates.pushMove(queued);
		}
	}

	/** @copydoc MeshGenerationSystem::cancelPlanetJobs */
	void MeshGenerationSystem::cancelPlanetJobs(PlanetId planet_id,
		uint64 planet_generation_id)
	{
		DSE_WATCH();
		cancelJobsForPlanet(planet_id, planet_generation_id);
		for(size_t index = 0; index < job_candidates.size();)
		{
			const MeshGenerationJob& candidate = job_candidates[index];
			if(candidate.source != MeshGenerationSource::planet_terrain_node
				|| candidate.planet_id != planet_id
				|| candidate.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < job_candidates.size(); shift++)
				job_candidates[shift - 1] = static_cast<MeshGenerationJob&&>(job_candidates[shift]);
			job_candidates.resize(job_candidates.size() - 1);
		}
	}

	void MeshGenerationSystem::invalidateBodyDetailMesh(WorldBodyId world_body_id)
	{
		DSE_WATCH();
		if(!world.getWorldBodyData().isWorldBodyActive(world_body_id)) return;
		WorldBody& body = world.getWorldBodyData().getWorldBody(world_body_id);
		body.detail_mesh_dirty = true;
		body.detail_mesh_queued = false;
		if(++body.detail_mesh_revision == 0) ++body.detail_mesh_revision;
	}

	/** @copydoc MeshGenerationSystem::collectBuildPartJobCandidates */
	void MeshGenerationSystem::collectBuildPartJobCandidates()
	{
		DSE_WATCH();
		for(WorldBodyId world_body_id : world.getWorldBodyData().getUsedWorldBodyIds())
		{
			WorldBody& body = world.getWorldBodyData().getWorldBody(world_body_id);
			if(!body.detail_mesh_dirty && !body.detail_mesh_queued) continue;
			if(body.detail_mesh_dirty)
			{
				if(body.detail_mesh.mesh != nullptr)
					world.getTelemetrySystem().mesh_generation_telemetry.recordMeshDeletion(
						MeshDeletionReason::detail_mesh_rebuild);
				destroyMeshRenderData(body.detail_mesh);
				body.detail_mesh_dirty = false;
				body.detail_mesh_queued = true;
			}

			MeshGenerationJob candidate;
			candidate.source = MeshGenerationSource::build_parts;
			candidate.generation_id = generation_id;
			candidate.source_revision = body.detail_mesh_revision;
			candidate.world_body_id = world_body_id;
			job_candidates.pushMove(candidate);
		}
	}

	/** @copydoc MeshGenerationSystem::frame */
	void MeshGenerationSystem::frame()
	{
		DSE_WATCH();
		collectBuildPartJobCandidates();
		MeshGenerationJob generated;
		if(tryTakeCompletedJob(generated) && generated.generation_id == generation_id
			&& generated.source == MeshGenerationSource::planet_terrain_node)
		{
			world.getPlanetSystem().acceptGeneratedSurfaceNode(generated);
		}
		else if(generated.generation_id == generation_id
			&& generated.source == MeshGenerationSource::build_parts)
		{
			if(world.getWorldBodyData().isWorldBodyActive(generated.world_body_id))
			{
				WorldBody& body = world.getWorldBodyData().getWorldBody(generated.world_body_id);
				if(body.detail_mesh_revision == generated.source_revision)
				{
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

		updateMeshGenerationJobs(generation_id, world.getPlanetSystem().buildMeshGenerationJobBuffer(
			job_candidates, thread_pool.workerCount() * 2));
	}
}
namespace danikk_space_engine::mesh_generation_internal
{
	namespace
	{
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

		bool sameJobKey(const MeshGenerationJob& a, const MeshGenerationJob& b)
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

		template<typename array_t>
		bool hasMatchingJob(const MeshGenerationJob& candidate, const array_t& jobs)
		{
			for(const MeshGenerationJob& job : jobs)
				if(job.state != MeshGenerationJobState::empty && sameJobKey(job, candidate)) return true;
			return false;
		}

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
				mesh_generation_internal::getPlanetTerrainNodeLevel(job.node_width),
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

	void MeshGenerationSystem::updateMeshGenerationJobs(uint64 new_generation_id,
		const DynamicArray<MeshGenerationJob>& desired_jobs)
	{
		PlanetMeshQueueTelemetry snapshot;
		{
			std::lock_guard<std::mutex> guard(task_mutex);
			if(active_generation_id != new_generation_id)
			{
				active_generation_id = new_generation_id;
				for(MeshGenerationJob& job : jobs)
				{
					if(job.state == MeshGenerationJobState::processing) job.cancelled = true;
					else job = MeshGenerationJob();
				}
			}

			const auto isDesired = [&](const MeshGenerationJob& job)
			{
				for(const MeshGenerationJob& candidate : desired_jobs)
					if(candidate.generation_id == active_generation_id && sameJobKey(job, candidate))
						return true;
				return false;
			};
			for(MeshGenerationJob& job : jobs)
			{
				if(job.state == MeshGenerationJobState::empty || isDesired(job)) continue;
				// A running task cannot be interrupted, so discarding its output only
				// forces the same node to be generated again if it becomes desired later.
				if(job.state == MeshGenerationJobState::queued) job = MeshGenerationJob();
			}

			for(const MeshGenerationJob& candidate : desired_jobs)
			{
				if(candidate.generation_id != active_generation_id || hasMatchingJob(candidate, jobs)) continue;
				for(MeshGenerationJob& job : jobs)
					if(job.state == MeshGenerationJobState::empty)
					{
						job = MeshGenerationJob(candidate);
						job.state = MeshGenerationJobState::queued;
						break;
					}
			}

			for(const MeshGenerationJob& candidate : desired_jobs)
			{
				if(candidate.source != MeshGenerationSource::planet_terrain_node) continue;
				snapshot.desired++;
				if(candidate.node_width <= 128.0) snapshot.fine_desired++;
			}
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
					if(!isDesired(job)) snapshot.active_not_desired++;
				}
				else if(job.state == MeshGenerationJobState::completed) snapshot.completed++;
			}
		}
		job_available.notify_all();
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
