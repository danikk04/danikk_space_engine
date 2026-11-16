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
		 * @brief Reports whether point active.
		 */
		bool isPointActive(PointId id) const;

		/**
		 * @brief Reports whether ground triangle active.
		 */
		bool isGroundTriangleActive(GroundTriangleId id) const;

		/**
		 * @brief Reports whether chunk active.
		 */
		bool isChunkActive(ChunkId id) const;

		/**
		 * @brief Reports whether world body active.
		 */
		bool isWorldBodyActive(WorldBodyId id) const;

		/**
		 * @brief Reports whether world body component slot active.
		 */
		bool isWorldBodyComponentSlotActive(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Reports whether point component slot active.
		 */
		bool isPointComponentSlotActive(PointComponentSlotId id) const;

		/**
		 * @brief Reports whether graph component slot active.
		 */
		bool isGraphComponentSlotActive(GraphComponentSlotId id) const;

		/**
		 * @brief Returns used point ids.
		 */
		const DynamicArray<PointId>& getUsedPointIds() const;

		/**
		 * @brief Returns used ground triangle ids.
		 */
		const DynamicArray<GroundTriangleId>& getUsedGroundTriangleIds() const;

		/**
		 * @brief Returns used chunk ids.
		 */
		const DynamicArray<ChunkId>& getUsedChunkIds() const;

		/**
		 * @brief Returns used world body ids.
		 */
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;

		/**
		 * @brief Returns used world body component slot ids.
		 */
		const DynamicArray<WorldBodyComponentSlotId>& getUsedWorldBodyComponentSlotIds() const;

		/**
		 * @brief Returns used point component slot ids.
		 */
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;

		/**
		 * @brief Returns used graph component slot ids.
		 */
		const DynamicArray<GraphComponentSlotId>& getUsedGraphComponentSlotIds() const;

		/**
		 * @brief Returns point.
		 */
		Point& getPoint(PointId id);

		/**
		 * @brief Returns point.
		 */
		const Point& getPoint(PointId id) const;

		/**
		 * @brief Returns ground triangle.
		 */
		GroundTriangle& getGroundTriangle(GroundTriangleId id);

		/**
		 * @brief Returns ground triangle.
		 */
		const GroundTriangle& getGroundTriangle(GroundTriangleId id) const;

		/**
		 * @brief Returns chunk.
		 */
		Chunk& getChunk(ChunkId id);

		/**
		 * @brief Returns chunk.
		 */
		const Chunk& getChunk(ChunkId id) const;

		/**
		 * @brief Returns world body.
		 */
		WorldBody& getWorldBody(WorldBodyId id);

		/**
		 * @brief Returns world body.
		 */
		const WorldBody& getWorldBody(WorldBodyId id) const;

		/**
		 * @brief Returns world body component slot.
		 */
		WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id);

		/**
		 * @brief Returns world body component slot.
		 */
		const WorldBodyComponentSlot& getWorldBodyComponentSlot(WorldBodyComponentSlotId id) const;

		/**
		 * @brief Returns point component slot.
		 */
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);

		/**
		 * @brief Returns point component slot.
		 */
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;

		/**
		 * @brief Returns graph component slot.
		 */
		GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id);

		/**
		 * @brief Returns graph component slot.
		 */
		const GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id) const;

		// These counts are total slot-array lengths, including free slots.

		/**
		 * @brief Returns point slot count.
		 */
		size_t getPointSlotCount() const;

		/**
		 * @brief Returns ground triangle slot count.
		 */
		size_t getGroundTriangleSlotCount() const;

		/**
		 * @brief Returns chunk slot count.
		 */
		size_t getChunkSlotCount() const;

		/**
		 * @brief Returns world body slot count.
		 */
		size_t getWorldBodySlotCount() const;

		/**
		 * @brief Returns world body component slot count.
		 */
		size_t getWorldBodyComponentSlotCount() const;

		/**
		 * @brief Returns point component slot count.
		 */
		size_t getPointComponentSlotCount() const;

		/**
		 * @brief Returns graph component slot count.
		 */
		size_t getGraphComponentSlotCount() const;

		/**
		 * @brief Performs the clear operation.
		 */
		void clear();

		/**
		 * @brief Performs the release operation.
		 */
		void release();
	};
}
