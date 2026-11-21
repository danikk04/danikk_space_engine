#pragma once

#include <memory>

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	/** @brief Enables the periodic log of average, minimum, and maximum mesh-generation times. */
	inline constexpr bool log_mesh_generation_statistics = false;

	namespace mesh_generation_internal { class Worker; }

	/** @brief Selects which world data source is converted into a mesh. */
	enum class MeshGenerationSource : uint8
	{
		/** @brief Generate an adaptive planet surface or cave octree cell. */
		planet_terrain_node,
		/** @brief Combine all structural BuildPart instances owned by a WorldBody. */
		build_parts
	};

	/** @brief CPU-only identifiers and generation metadata consumed by a mesh worker. */
	struct MeshGenerationRequest
	{
		uint64 generation_id = 0;
		uint64 source_revision = 0;
		WorldBodyId world_body_id = 0;
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
	 * The planet octree and WorldBody own accepted meshes. This system manages requests,
	 * worker lifecycle, stale-result rejection, and main-thread uploads.
	 */
	class MeshGenerationSystem
	{
		friend class mesh_generation_internal::Worker;

		/** @brief World providing source data and receiving generated meshes. */
		World& world;
		/** @brief Owns this world's CPU mesh worker and its request/result queues. */
		std::unique_ptr<mesh_generation_internal::Worker> worker;
		/** @brief Token that rejects results submitted before the latest World lifecycle. */
		uint64 generation_id = 0;
		/** @brief Latest desired generation requests waiting for worker submission. */
		DynamicArray<MeshGenerationRequest> requests;
		/** @brief True when queued request metadata differs from the worker's submitted queue. */
		bool requests_dirty = false;
		/** @brief True while the worker has been started for the current World lifecycle. */
		bool initialized = false;

		/** @brief Adds or replaces dirty combined BuildPart mesh requests. */
		void queueDirtyBuildPartMeshes();
		/** @brief Removes the queued BuildPart mesh request for one body. */
		void removeBuildPartMeshRequest(WorldBodyId world_body_id);

	public:
		explicit MeshGenerationSystem(World& world);
		~MeshGenerationSystem();
		MeshGenerationSystem(const MeshGenerationSystem&) = delete;
		MeshGenerationSystem& operator=(const MeshGenerationSystem&) = delete;

		/** @brief Starts the worker and resets request state for a fresh World lifetime. */
		void init();
		/** @brief Stops the worker, invalidates outstanding work, and clears queued requests. */
		void shutdown();
		/** @brief Submits current requests and accepts at most one completed mesh on the main thread. */
		void frame();
		/** @brief Replaces the queued terrain requests for one planet incarnation. */
		void replacePlanetRequests(WorldBodyId world_body_id, uint64 planet_generation_id,
			const DynamicArray<MeshGenerationRequest>& planet_requests);
		/** @brief Cancels all queued and worker-side requests for one planet incarnation. */
		void cancelPlanetRequests(WorldBodyId world_body_id, uint64 planet_generation_id);
		/** @brief Marks the owning body's combined BuildPart mesh stale. */
		void invalidateBodyDetailMesh(WorldBodyId world_body_id);
		/** @brief Reads the latest average generation time over 50 completed mesh jobs. */
		bool getAverageGenerationTimeMilliseconds(float& average_time) const;
	};
}
