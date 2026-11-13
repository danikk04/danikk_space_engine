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

	PointComponentSlotId WorldData::createPointComponentSlot(
		PointId point_id,
		PointComponentTypeId type_id,
		const void* init_data)
	{
		if(point_id >= points.size())
		{
			danikk_engine::formatFatalError("Point component references invalid point %", point_id);
		}

		const PointComponentType& type = getPointComponentType(type_id);
		if(type_id == PointComponentTypeId::none || type.construct == nullptr || type.data_size == 0)
		{
			danikk_engine::formatFatalError("Point component type % cannot be constructed", static_cast<uint32>(type_id));
		}
		if(type.data_alignment > alignof(std::max_align_t))
		{
			danikk_engine::formatFatalError("Point component type % requires unsupported alignment", static_cast<uint32>(type_id));
		}

		const PointComponentSlotId id = static_cast<PointComponentSlotId>(point_components.size());
		PointComponentSlot& slot = point_components.pushCtor();
		slot.point_id = point_id;
		slot.type_id = type_id;
		slot.uses_external_data = !type.inline_relocatable
			|| type.data_size > PointComponentSlot::inline_data_capacity;
		if(slot.uses_external_data)
		{
			slot.external_data = (danikk_framework::malloc)(type.data_size);
			if(slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Cannot allocate % bytes for point component type %", type.data_size, static_cast<uint32>(type_id));
			}
		}
		type.construct(slot.getData(), init_data);
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
