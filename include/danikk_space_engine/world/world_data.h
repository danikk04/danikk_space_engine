#pragma once

#include <danikk_space_engine/point.h>
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

	public:
		PointId createPoint();
		VolumeCellId createVolumeCell();
		Chunk& createChunk();

		Point& getPoint(PointId id);
		const Point& getPoint(PointId id) const;
		VolumeCell& getVolumeCell(VolumeCellId id);
		const VolumeCell& getVolumeCell(VolumeCellId id) const;
		Chunk& getChunk(ChunkId id);
		const Chunk& getChunk(ChunkId id) const;

		size_t getPointCount() const;
		size_t getVolumeCellCount() const;
		size_t getChunkCount() const;

		void clear();
		void release();
	};
}
