#include <danikk_space_engine/world/world_data.h>

#include <cstdlib>

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

	PointComponentSlotId WorldData::createPointComponentSlot()
	{
		const PointComponentSlotId id = static_cast<PointComponentSlotId>(point_components.size());
		point_components.pushCtor();
		return id;
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

	PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id)
	{
		return point_components[id];
	}

	const PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) const
	{
		return point_components[id];
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

	size_t WorldData::getPointComponentSlotCount() const
	{
		return point_components.size();
	}

	void WorldData::clear()
	{
		for(PointComponentSlot& slot : point_components)
		{
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot has no external data during cleanup");
			}

			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(type.destroy != nullptr)
			{
				type.destroy(slot.getData());
			}
			else if(slot.type_id != PointComponentTypeId::none)
			{
				danikk_engine::formatFatalError("Point component type % has no destroy function", static_cast<uint32>(slot.type_id));
			}

			if(slot.uses_external_data)
			{
				free(slot.external_data);
			}
		}
		point_components.clear();
		chunks.clear();
		points.clear();
		volume_cells.clear();
	}

	void WorldData::release()
	{
		clear();
		chunks = DynamicArray<Chunk>();
		point_components = DynamicArray<PointComponentSlot>();
		points = DynamicArray<Point>();
		volume_cells = DynamicArray<VolumeCell>();
	}
}
