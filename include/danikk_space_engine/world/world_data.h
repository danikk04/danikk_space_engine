#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/component/point.h>
#include <danikk_space_engine/world/volume_cell.h>
#include <danikk_space_engine/world/chunk.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class WorldData
	{
		DynamicArray<Point> points;
		DynamicArray<VolumeCell> volume_cells;
		DynamicArray<Chunk> chunks;
		DynamicArray<PointComponentSlot> point_components;
		DynamicArray<PointId> used_point_ids;
		DynamicArray<VolumeCellId> used_volume_cell_ids;
		DynamicArray<ChunkId> used_chunk_ids;
		DynamicArray<PointComponentSlotId> used_point_component_slot_ids;
		DynamicArray<PointId> freed_point_ids;
		DynamicArray<VolumeCellId> freed_volume_cell_ids;
		DynamicArray<ChunkId> freed_chunk_ids;
		DynamicArray<PointComponentSlotId> freed_point_component_slot_ids;

	public:
		PointId createPoint();
		VolumeCellId createVolumeCell();
		Chunk& createChunk();
		PointComponentSlotId createPointComponentSlot(
			PointId point_id,
			PointComponentTypeId type_id,
			const void* init_data = nullptr);

		// Point deletion also destroys attached components and cells that reference it.
		void destroyPoint(PointId id);
		// Cell deletion removes its ID from every chunk that contains it.
		void destroyVolumeCell(VolumeCellId id);
		// Chunk deletion destroys its component slots and volume cells, but leaves shared points.
		void destroyChunk(ChunkId id);
		void destroyPointComponentSlot(PointComponentSlotId id);

		bool isPointActive(PointId id) const;
		bool isVolumeCellActive(VolumeCellId id) const;
		bool isChunkActive(ChunkId id) const;
		bool isPointComponentSlotActive(PointComponentSlotId id) const;
		const DynamicArray<PointId>& getUsedPointIds() const;
		const DynamicArray<VolumeCellId>& getUsedVolumeCellIds() const;
		const DynamicArray<ChunkId>& getUsedChunkIds() const;
		const DynamicArray<PointComponentSlotId>& getUsedPointComponentSlotIds() const;

		Point& getPoint(PointId id);
		const Point& getPoint(PointId id) const;
		VolumeCell& getVolumeCell(VolumeCellId id);
		const VolumeCell& getVolumeCell(VolumeCellId id) const;
		Chunk& getChunk(ChunkId id);
		const Chunk& getChunk(ChunkId id) const;
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;

		// These counts are total slot-array lengths, including free slots.
		size_t getPointCount() const;
		size_t getVolumeCellCount() const;
		size_t getChunkCount() const;
		size_t getPointComponentSlotCount() const;

		void clear();
		void release();
	};
}
