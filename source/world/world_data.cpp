#include <danikk_space_engine/world/world_data.h>

namespace danikk_space_engine
{
	PointId WorldData::createPoint()
	{
		const PointId id = static_cast<PointId>(points.size());
		points.pushCtor();
		return id;
	}

	VolumeCellId WorldData::createVolumeCell()
	{
		const VolumeCellId id = static_cast<VolumeCellId>(volume_cells.size());
		volume_cells.pushCtor();
		return id;
	}

	Chunk& WorldData::createChunk()
	{
		return chunks.pushCtor();
	}

	Point& WorldData::getPoint(PointId id)
	{
		return points[id];
	}

	const Point& WorldData::getPoint(PointId id) const
	{
		return points[id];
	}

	VolumeCell& WorldData::getVolumeCell(VolumeCellId id)
	{
		return volume_cells[id];
	}

	const VolumeCell& WorldData::getVolumeCell(VolumeCellId id) const
	{
		return volume_cells[id];
	}

	Chunk& WorldData::getChunk(ChunkId id)
	{
		return chunks[id];
	}

	const Chunk& WorldData::getChunk(ChunkId id) const
	{
		return chunks[id];
	}

	size_t WorldData::getPointCount() const
	{
		return points.size();
	}

	size_t WorldData::getVolumeCellCount() const
	{
		return volume_cells.size();
	}

	size_t WorldData::getChunkCount() const
	{
		return chunks.size();
	}

	void WorldData::clear()
	{
		chunks.clear();
		points.clear();
		volume_cells.clear();
	}

	void WorldData::release()
	{
		chunks = DynamicArray<Chunk>();
		points = DynamicArray<Point>();
		volume_cells = DynamicArray<VolumeCell>();
	}
}
