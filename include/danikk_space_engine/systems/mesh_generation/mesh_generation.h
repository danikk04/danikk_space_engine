#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <danikk_framework/dynamic_array.h>
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

	/** @brief CPU-only identifiers and generation metadata consumed by a mesh task. */
	struct MeshGenerationRequest
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
	};

	/** @brief CPU mesh payload returned to the main thread for validation and GPU upload. */
	struct GeneratedMesh
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
		glm::dvec3 node_minimum = glm::dvec3(0.0);
		glm::dvec3 mesh_position = glm::dvec3(0.0);
		DynamicArray<vec3> point_positions;
		DynamicArray<uint32> triangle_point_indices;
		DynamicArray<MaterialId> triangle_material_ids;
	};

	/**
	 * @brief Owns asynchronous mesh requests and accepts generated CPU meshes.
	 *
	 * PlanetData and WorldBody own their accepted meshes. This system manages requests,
	 * task-pool lifecycle, stale-result rejection, and main-thread uploads.
	 */
	class MeshGenerationSystem
	{
		/** @brief World providing source data and receiving generated meshes. */
		World& world;
		struct TaskRequestSlot
		{
			MeshGenerationRequest request;
			bool has_request = false;
			bool cancelled = false;
		};
		/** @brief Pool tasks consume the shared request queue, one request at a time. */
		std::unique_ptr<danikk_framework::ThreadPool> thread_pool;
		std::mutex task_mutex;
		std::condition_variable request_available;
		std::condition_variable result_space_available;
		DynamicArray<MeshGenerationRequest> queued_requests;
		DynamicArray<GeneratedMesh> completed_meshes;
		DynamicArray<TaskRequestSlot> task_request_slots;
		uint32 generation_sample_count = 0;
		uint64 generation_sample_total_nanoseconds = 0;
		std::atomic<float> average_generation_milliseconds = 0.0f;
		bool stopping_tasks = false;
		uint64 active_generation_id = 0;
		/** @brief Token that rejects results submitted before the latest World lifecycle. */
		uint64 generation_id = 0;
		/** @brief Current candidate requests collected from terrain and BuildPart systems. */
		DynamicArray<MeshGenerationRequest> requests;
		/** @brief True while the task pool is running for the current World lifecycle. */
		bool initialized = false;

		/** @brief Rebuilds current combined BuildPart requests from body mesh state. */
		void collectBuildPartRequests();
		size_t findActiveRequest(const MeshGenerationRequest& request) const;
		bool isActiveRequest(const MeshGenerationRequest& request) const;
		void clearTaskRequestSlot(size_t task_index);
		void runMeshGenerationTask(size_t task_index);
		void generateMeshRequest(size_t task_index, const MeshGenerationRequest& request);
		void updateMeshGenerationRequests(uint64 generation_id,
			const DynamicArray<MeshGenerationRequest>& requests);
		bool tryTakeGeneratedMesh(GeneratedMesh& result);
		void cancelQueuedPlanetRequests(PlanetId planet_id, uint64 planet_generation_id);

	public:
		explicit MeshGenerationSystem(World& world);
		~MeshGenerationSystem();
		MeshGenerationSystem(const MeshGenerationSystem&) = delete;
		MeshGenerationSystem& operator=(const MeshGenerationSystem&) = delete;

		/** @brief Starts the task pool and resets request state for a fresh World lifetime. */
		void init();
		/** @brief Stops the task pool, invalidates outstanding work, and clears queued requests. */
		void shutdown();
		/** @brief Submits current requests and accepts at most one completed mesh on the main thread. */
		void frame();
		/** @brief Clears the candidate list before systems rebuild requests for a new frame. */
		void beginRequestFrame();
		/** @brief Appends this frame's terrain candidates to the shared candidate list. */
		void appendPlanetRequests(const DynamicArray<MeshGenerationRequest>& planet_requests);
		/** @brief Cancels queued and active task requests for one planet incarnation. */
		void cancelPlanetRequests(PlanetId planet_id, uint64 planet_generation_id);
		/** @brief Marks the owning body's combined BuildPart mesh stale. */
		void invalidateBodyDetailMesh(WorldBodyId world_body_id);
		/** @brief Reads the latest average generation time over 50 completed mesh jobs. */
		bool getAverageGenerationTimeMilliseconds(float& average_time) const;
	};
}
