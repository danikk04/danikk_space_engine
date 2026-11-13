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

	public:
		PointId createPoint();
		VolumeCellId createVolumeCell();
		Chunk& createChunk();
	PointComponentSlotId createPointComponentSlot(
		PointId point_id,
		PointComponentTypeId type_id,
		const void* init_data = nullptr);

		Point& getPoint(PointId id);
		const Point& getPoint(PointId id) const;
		VolumeCell& getVolumeCell(VolumeCellId id);
		const VolumeCell& getVolumeCell(VolumeCellId id) const;
		Chunk& getChunk(ChunkId id);
		const Chunk& getChunk(ChunkId id) const;
		PointComponentSlot& getPointComponentSlot(PointComponentSlotId id);
		const PointComponentSlot& getPointComponentSlot(PointComponentSlotId id) const;

		size_t getPointCount() const;
		size_t getVolumeCellCount() const;
		size_t getChunkCount() const;
		size_t getPointComponentSlotCount() const;

		void clear();
		void release();
	};
}
