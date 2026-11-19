#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/chunk_mesh_generator.h>

namespace danikk_space_engine
{
	/**
	 * @brief Owns asynchronous chunk-mesh requests and accepts generated CPU meshes.
	 *
	 * ChunkSystem retains chunk and GPU-mesh ownership. This system manages request
	 * snapshots, generator lifecycle, stale-result rejection, and main-thread uploads.
	 */
	class ChunkMeshGenerationSystem
	{
		/** @brief World providing source data and receiving generated meshes. */
		World& world;
		/** @brief Token that rejects results submitted before the latest World lifecycle. */
		uint64 generation_id = 0;
		/** @brief Latest desired generation requests waiting for worker submission. */
		DynamicArray<ChunkMeshGenerationRequest> requests;
		/** @brief True when the request snapshot differs from the worker's submitted snapshot. */
		bool requests_dirty = false;

		/** @brief Adds or replaces dirty BuildPart mesh requests from loaded chunks. */
		void queueDirtyBuildPartMeshes();
		/** @brief Removes the queued BuildPart mesh request for one chunk. */
		void removeBuildPartMeshRequest(ChunkId chunk_id);

	public:
		explicit ChunkMeshGenerationSystem(World& world) : world(world) {}
		ChunkMeshGenerationSystem(const ChunkMeshGenerationSystem&) = delete;
		ChunkMeshGenerationSystem& operator=(const ChunkMeshGenerationSystem&) = delete;

		/** @brief Starts the worker and resets request state for a fresh World lifetime. */
		void init();
		/** @brief Stops the worker, invalidates outstanding work, and clears queued requests. */
		void shutdown();
		/** @brief Submits current requests and accepts at most one completed mesh on the main thread. */
		void frame();
		/** @brief Replaces the queued terrain requests for one planet incarnation. */
		void replacePlanetRequests(WorldBodyId world_body_id, uint64 planet_generation_id,
			const DynamicArray<ChunkMeshGenerationRequest>& planet_requests);
		/** @brief Removes a terrain request for one planet grid coordinate. */
		void removePlanetRequest(WorldBodyId world_body_id, uint64 planet_generation_id,
			const glm::ivec3& grid_coordinate);
		/** @brief Cancels all queued and worker-side requests for one planet incarnation. */
		void cancelPlanetRequests(WorldBodyId world_body_id, uint64 planet_generation_id);
	};
}
