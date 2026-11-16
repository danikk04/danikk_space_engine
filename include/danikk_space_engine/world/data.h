#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/graph/graph.h>
#include <danikk_space_engine/world/ground_triangle.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/body.h>
#include <danikk_space_engine/world/resource_pool.h>
#include <danikk_space_engine/component/world_body/world_body.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns indexed pools for points, triangles, chunks, world bodies, and component slots.
	 */
	class WorldData
	{
		WorldResourcePool<Point, PointId> points{"Point"};
		WorldResourcePool<GroundTriangle, GroundTriangleId> ground_triangles{"GroundTriangle"};
		WorldResourcePool<Chunk, ChunkId> chunks{"Chunk"};
		WorldResourcePool<WorldBody, WorldBodyId> world_bodies{"WorldBody"};
		WorldResourcePool<WorldBodyComponentSlot, WorldBodyComponentSlotId> world_body_components{"WorldBodyComponentSlot"};
		WorldResourcePool<PointComponentSlot, PointComponentSlotId> point_components{"PointComponentSlot"};
		WorldResourcePool<GraphComponentSlot, GraphComponentSlotId> graph_components{"GraphComponentSlot"};
		uint64 next_world_body_component_generation_id = 1;

	public:

		/**
		 * @brief Creates point.
		 */
		PointId createPoint();

		/**
		 * @brief Creates ground triangle.
		 */
		GroundTriangleId createGroundTriangle();

		/**
		 * @brief Creates world body.
		 */
		WorldBodyId createWorldBody();

		/**
		 * @brief Creates world body component slot.
		 */
		WorldBodyComponentSlotId createWorldBodyComponentSlot(
			WorldBodyId world_body_id,
			WorldBodyComponentTypeId type_id,
			const void* init_data = nullptr);

		/**
		 * @brief Creates chunk.
		 */
		Chunk& createChunk(WorldBodyId world_body_id);

		/**
		 * @brief Adds point reference to chunk.
		 */
		void addPointReferenceToChunk(ChunkId chunk_id, PointId point_id);

		/**
		 * @brief Adds ground triangle to chunk.
		 */
		void addGroundTriangleToChunk(ChunkId chunk_id, GroundTriangleId triangle_id);

		/**
		 * @brief Adds point component slot to chunk.
		 */
		void addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id);

		/**
		 * @brief Creates point component slot.
		 */
		PointComponentSlotId createPointComponentSlot(
			PointId point_id,
			PointComponentTypeId type_id,
			const void* init_data = nullptr);

		/**
		 * @brief Creates graph component slot.
		 */
		GraphComponentSlotId createGraphComponentSlot(
			ChunkId chunk_id,
			PointId point_a,
			PointId point_b,
			GraphComponentTypeId type_id,
			const void* init_data = nullptr);

		// Point deletion also destroys attached components and triangles that reference it.

		/**
		 * @brief Removes and releases point.
		 */
		void destroyPoint(PointId id);
		// Triangles store their owner chunk and slot for direct removal.

		/**
		 * @brief Removes and releases ground triangle.
		 */
		void destroyGroundTriangle(GroundTriangleId id);
		// Chunk deletion destroys its components and geometry; points remain active while any chunk references them.

		/**
		 * @brief Removes and releases chunk.
		 */
		void destroyChunk(ChunkId id);

		/**
		 * @brief Removes and releases world body.
		 */
		void destroyWorldBody(WorldBodyId id);

		/**
		 * @brief Removes and releases world body component slot.
		 */
		void destroyWorldBodyComponentSlot(WorldBodyComponentSlotId id);

		/**
		 * @brief Removes and releases point component slot.
		 */
		void destroyPointComponentSlot(PointComponentSlotId id);

		/**
		 * @brief Removes and releases graph component slot.
		 */
		void destroyGraphComponentSlot(GraphComponentSlotId id);

		/**
		 * @brief Checks whether the point ID currently refers to an allocated point.
		 */
		bool isPointActive(PointId id) const;

		/**
		 * @brief Checks whether the triangle ID currently refers to an allocated ground triangle.
		 */
		bool isGroundTriangleActive(GroundTriangleId id) const;

		/**
		 * @brief Checks whether the chunk ID currently refers to an allocated chunk.
		 */
		bool isChunkActive(ChunkId id) const;

		/**
		 * @brief Checks whether the world-body ID currently refers to an allocated body.
		 */
		bool isWorldBodyActive(WorldBodyId id) const;

		/**
		 * @brief Checks whether the ID refers to an allocated world-body component slot.
		 */
		bool isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Checks whether the ID refers to an allocated point component slot.
		 */
		bool isPointComponentSlotActive(PointComponentSlotId id) const;

		/**
		 * @brief Checks whether the ID refers to an allocated graph component slot.
		 */
		bool isGraphComponentSlotActive(GraphComponentSlotId id) const;

		/**
		 * @brief Returns the pool-owned list of active point IDs; do not retain it across pool mutations.
		 */
		const DynamicArray<PointId>& getUsedPointIds() const;

		/**
		 * @brief Returns the pool-owned list of active terrain-triangle IDs; do not retain it across pool mutations.
		 */
		const DynamicArray<GroundTriangleId>& getUsedGroundTriangleIds() const;

		/**
		 * @brief Returns the pool-owned list of active chunk IDs; do not retain it across pool mutations.
		 */
		const DynamicArray<ChunkId>& getUsedChunkIds() const;

		/**
		 * @brief Returns the pool-owned list of active world-body IDs; do not retain it across pool mutations.
		 */
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;

		/**
		 * @brief Returns the pool-owned list of active world-body component slot IDs.
		 */
		const DynamicArray<WorldBodyComponentSlotId>& getUsedWorldBodyComponentSlotIds() const;

		/**
		 * @brief Returns the pool-owned list of active point component slot IDs.
		 */
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;

		/**
		 * @brief Returns the pool-owned list of active graph component slot IDs.
		 */
		const DynamicArray<GraphComponentSlotId>& getUsedGraphComponentSlotIds() const;

		/**
		 * @brief Returns a checked reference to an active point; an invalid ID raises a fatal error.
		 */
		Point& getPoint(PointId id);

		/**
		 * @brief Returns a checked reference to an active point; an invalid ID raises a fatal error.
		 */
		const Point& getPoint(PointId id) const;

		/**
		 * @brief Returns a checked reference to an active ground triangle; an invalid ID raises a fatal error.
		 */
		GroundTriangle& getGroundTriangle(GroundTriangleId id);

		/**
		 * @brief Returns a checked reference to an active ground triangle; an invalid ID raises a fatal error.
		 */
		const GroundTriangle& getGroundTriangle(GroundTriangleId id) const;

		/**
		 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
		 */
		Chunk& getChunk(ChunkId id);

		/**
		 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
		 */
		const Chunk& getChunk(ChunkId id) const;

		/**
		 * @brief Returns a checked reference to an active world body; an invalid ID raises a fatal error.
		 */
		WorldBody& getWorldBody(WorldBodyId id);

		/**
		 * @brief Returns a checked reference to an active world body; an invalid ID raises a fatal error.
		 */
		const WorldBody& getWorldBody(WorldBodyId id) const;

		/**
		 * @brief Returns a checked reference to an active world-body component slot.
		 */
		WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id);

		/**
		 * @brief Returns a checked reference to an active world-body component slot.
		 */
		const WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Returns a checked reference to an active point component slot.
		 */
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);

		/**
		 * @brief Returns a checked reference to an active point component slot.
		 */
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;

		/**
		 * @brief Returns a checked reference to an active graph component slot.
		 */
		GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id);

		/**
		 * @brief Returns a checked reference to an active graph component slot.
		 */
		const GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id) const;

		// These counts are total slot-array lengths, including free slots.

		/**
		 * @brief Returns the total point-pool slot count, including slots available for reuse.
		 */
		size_t getPointSlotCount() const;

		/**
		 * @brief Returns the total ground-triangle pool slot count, including reusable slots.
		 */
		size_t getGroundTriangleSlotCount() const;

		/**
		 * @brief Returns the total chunk-pool slot count, including reusable slots.
		 */
		size_t getChunkSlotCount() const;

		/**
		 * @brief Returns the total world-body pool slot count, including reusable slots.
		 */
		size_t getWorldBodySlotCount() const;

		/**
		 * @brief Returns the total world-body component pool slot count.
		 */
		size_t getWorldBodyComponentSlotCount() const;

		/**
		 * @brief Returns the total point-component pool slot count.
		 */
		size_t getPointComponentSlotCount() const;

		/**
		 * @brief Returns the total graph-component pool slot count.
		 */
		size_t getGraphComponentSlotCount() const;

		/**
		 * @brief Resets the inventory slot to its empty, unequipped state.
		 */
		void clear();

		/**
		 * @brief Releases the backing storage after destroying all resources in the pool.
		 */
		void release();
	};
}
