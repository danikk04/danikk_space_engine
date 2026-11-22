#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <danikk_framework/dynamic_array.h>
#include <danikk_framework/array.h>
#include <danikk_framework/thread.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	/** @brief Selects which world data source is converted into a mesh. */
	enum class MeshGenerationSource : uint8
	{
		/** @brief Generate an adaptive planet surface or cave octree cell. */
		planet_terrain_node,
		/** @brief Combine all structural BuildPart instances owned by a WorldBody. */
		build_parts
	};

	enum class MeshGenerationJobState : uint8
	{
		empty,
		queued,
		processing,
		completed
	};

	/** @brief Mesh job candidate, reusable slot state, and generated CPU mesh payload. */
	struct MeshGenerationJob
	{
		uint64 generation_id = 0;
		uint64 source_revision = 0;
		WorldBodyId world_body_id = 0;
		PlanetId planet_id = static_cast<PlanetId>(-1);
		uint64 planet_generation_id = 0;
		uint64 terrain_revision = 0;
		PlanetTerrainNodeId terrain_node_id = static_cast<PlanetTerrainNodeId>(-1);
		uint64 terrain_node_generation_id = 0;
		bool surface_only = false;
		MeshGenerationSource source = MeshGenerationSource::build_parts;
		PlanetMeshGenerator planet_mesh_generator = PlanetMeshGenerator::marching_tetrahedra;
		double node_width = 32.0;
		double voxel_size = 4.0;
		glm::dvec3 node_minimum = glm::dvec3(0.0);

		glm::dvec3 mesh_position = glm::dvec3(0.0);
		DynamicArray<vec3> point_positions;
		DynamicArray<uint32> triangle_point_indices;
		DynamicArray<MaterialId> triangle_material_ids;
		MeshGenerationJobState state = MeshGenerationJobState::empty;
		bool cancelled = false;
	};

	/**
	 * @brief Owns asynchronous mesh jobs and accepts generated CPU meshes.
	 *
	 * PlanetData and WorldBody own their accepted meshes. This system manages candidate
	 * collection, task-pool lifecycle, stale-job rejection, and main-thread uploads.
	 */
	class MeshGenerationSystem
	{
		/** @brief World providing source data and receiving generated meshes. */
		World& world;
		/** @brief Worker threads execute jobs from the fixed reusable slot array. */
		danikk_framework::ThreadPool thread_pool;
		std::mutex task_mutex;
		std::condition_variable job_available;
		/** @brief Two reusable candidate/output slots per worker; all state changes use task_mutex. */
		Array<MeshGenerationJob, 8> jobs;
		uint32 generation_sample_count = 0;
		uint64 generation_sample_total_nanoseconds = 0;
		std::atomic<float> average_generation_milliseconds = 0.0f;
		bool stopping_tasks = false;
		uint64 active_generation_id = 0;
		/** @brief Token that rejects jobs submitted before the latest World lifecycle. */
		uint64 generation_id = 0;
		/** @brief Current job candidates collected from terrain and BuildPart systems. */
		DynamicArray<MeshGenerationJob> job_candidates;
		/** @brief True while the task pool is running for the current World lifecycle. */
		bool initialized = false;

		/** @brief Rebuilds current BuildPart job candidates from body mesh state. */
		void collectBuildPartJobCandidates();
		void runMeshGenerationTask();
		void runMeshGenerationJob(size_t job_index);
		void updateMeshGenerationJobs(uint64 generation_id,
			const DynamicArray<MeshGenerationJob>& job_candidates);
		bool tryTakeCompletedJob(MeshGenerationJob& completed_job);
		void cancelJobsForPlanet(PlanetId planet_id, uint64 planet_generation_id);

	public:
		explicit MeshGenerationSystem(World& world);
		~MeshGenerationSystem();
		MeshGenerationSystem(const MeshGenerationSystem&) = delete;
		MeshGenerationSystem& operator=(const MeshGenerationSystem&) = delete;

		/** @brief Starts the task pool and resets candidate state for a fresh World lifetime. */
		void init();
		/** @brief Stops the task pool, invalidates outstanding work, and clears pending jobs. */
		void shutdown();
		/** @brief Schedules current job candidates and accepts at most one completed mesh per frame. */
		void frame();
		/** @brief Clears the candidate list before systems rebuild it for a new frame. */
		void beginJobCandidateFrame();
		/** @brief Appends this frame's terrain candidates to the shared candidate list. */
		void appendPlanetJobCandidates(const DynamicArray<MeshGenerationJob>& planet_job_candidates);
		/** @brief Cancels queued and active jobs for one planet incarnation. */
		void cancelPlanetJobs(PlanetId planet_id, uint64 planet_generation_id);
		/** @brief Marks the owning body's combined BuildPart mesh stale. */
		void invalidateBodyDetailMesh(WorldBodyId world_body_id);
		/** @brief Reads the latest average generation time over 50 completed mesh jobs. */
		bool getAverageGenerationTimeMilliseconds(float& average_time) const;
	};
}
