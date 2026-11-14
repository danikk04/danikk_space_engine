#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/graph/graph.h>
#include <danikk_space_engine/world/volume_cell.h>
#include <danikk_space_engine/world/ground_triangle.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/body.h>
#include <danikk_space_engine/world/resource_pool.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class WorldData
	{
		WorldResourcePool<Point, PointId> points;
		WorldResourcePool<VolumeCell, VolumeCellId> volume_cells;
		WorldResourcePool<GroundTriangle, GroundTriangleId> ground_triangles;
		WorldResourcePool<Chunk, ChunkId> chunks;
		WorldResourcePool<WorldBody, WorldBodyId> world_bodies;
		WorldResourcePool<PointComponentSlot, PointComponentSlotId> point_components;
		WorldResourcePool<GraphComponentSlot, GraphComponentSlotId> graph_components;

	public:
		PointId createPoint();
		VolumeCellId createVolumeCell();
		GroundTriangleId createGroundTriangle();
		WorldBodyId createWorldBody();
		Chunk& createChunk(WorldBodyId world_body_id);
		void addPointReferenceToChunk(ChunkId chunk_id, PointId point_id);
		void addVolumeCellToChunk(ChunkId chunk_id, VolumeCellId cell_id);
		void addGroundTriangleToChunk(ChunkId chunk_id, GroundTriangleId triangle_id);
		void addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id);
		PointComponentSlotId createPointComponentSlot(
			PointId point_id,
			PointComponentTypeId type_id,
			const void* init_data = nullptr);
		GraphComponentSlotId createGraphComponentSlot(
			ChunkId chunk_id,
			PointId point_a,
			PointId point_b,
			GraphComponentTypeId type_id,
			const void* init_data = nullptr);

		// Point deletion also destroys attached components and cells that reference it.
		void destroyPoint(PointId id);
		// Cell deletion removes its ID from every chunk that contains it.
		void destroyVolumeCell(VolumeCellId id);
		void destroyGroundTriangle(GroundTriangleId id);
		// Chunk deletion destroys its component slots and volume cells, but leaves shared points.
		void destroyChunk(ChunkId id);
		void destroyWorldBody(WorldBodyId id);
		void destroyPointComponentSlot(PointComponentSlotId id);
		void destroyGraphComponentSlot(GraphComponentSlotId id);

		bool isPointActive(PointId id) const;
		bool isVolumeCellActive(VolumeCellId id) const;
		bool isGroundTriangleActive(GroundTriangleId id) const;
		bool isChunkActive(ChunkId id) const;
		bool isWorldBodyActive(WorldBodyId id) const;
		bool isPointComponentSlotActive(PointComponentSlotId id) const;
		bool isGraphComponentSlotActive(GraphComponentSlotId id) const;
		const DynamicArray<PointId>& getUsedPointIds() const;
		const DynamicArray<VolumeCellId>& getUsedVolumeCellIds() const;
		const DynamicArray<GroundTriangleId>& getUsedGroundTriangleIds() const;
		const DynamicArray<ChunkId>& getUsedChunkIds() const;
		const DynamicArray<WorldBodyId>& getUsedWorldBodyIds() const;
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;
		const DynamicArray<GraphComponentSlotId>& getUsedGraphComponentSlotIds() const;

		Point& getPoint(PointId id);
		const Point& getPoint(PointId id) const;
		VolumeCell& getVolumeCell(VolumeCellId id);
		const VolumeCell& getVolumeCell(VolumeCellId id) const;
		GroundTriangle& getGroundTriangle(GroundTriangleId id);
		const GroundTriangle& getGroundTriangle(GroundTriangleId id) const;
		Chunk& getChunk(ChunkId id);
		const Chunk& getChunk(ChunkId id) const;
		WorldBody& getWorldBody(WorldBodyId id);
		const WorldBody& getWorldBody(WorldBodyId id) const;
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;
		GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id);
		const GraphComponentSlot& getGraphComponentSlot(GraphComponentSlotId id) const;

		// These counts are total slot-array lengths, including free slots.
		size_t getPointCount() const;
		size_t getVolumeCellCount() const;
		size_t getGroundTriangleCount() const;
		size_t getChunkCount() const;
		size_t getWorldBodyCount() const;
		size_t getPointComponentSlotCount() const;
		size_t getGraphComponentSlotCount() const;

		void clear();
		void release();
	};
}
