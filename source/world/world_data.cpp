#include <danikk_space_engine/world/world_data.h>

#include <cstdlib>

namespace danikk_space_engine
{
	template<typename id_t>
	static size_t findSortedId(const DynamicArray<id_t>& ids, id_t id)
	{
		size_t first = 0;
		size_t last = ids.size();
		while(first < last)
		{
			const size_t middle = first + (last - first) / 2;
			if(ids[middle] < id) first = middle + 1;
			else last = middle;
		}
		return first;
	}

	template<typename id_t>
	static void insertSortedId(DynamicArray<id_t>& ids, id_t id)
	{
		const size_t index = findSortedId(ids, id);
		ids.resize(ids.size() + 1);
		for(size_t shift_index = ids.size() - 1; shift_index > index; shift_index--)
		{
			ids[shift_index] = ids[shift_index - 1];
		}
		ids[index] = id;
	}

	template<typename id_t>
	static bool eraseSortedId(DynamicArray<id_t>& ids, id_t id)
	{
		const size_t index = findSortedId(ids, id);
		if(index == ids.size() || ids[index] != id) return false;
		for(size_t shift_index = index; shift_index + 1 < ids.size(); shift_index++)
		{
			ids[shift_index] = ids[shift_index + 1];
		}
		ids.resize(ids.size() - 1);
		return true;
	}

	PointId WorldData::createPoint()
	{
		if(freed_point_ids.size() > 0)
		{
			const PointId id = freed_point_ids.pop();
			points[id] = Point();
			insertSortedId(used_point_ids, id);
			return id;
		}
		const PointId id = static_cast<PointId>(points.size());
		points.pushCtor();
		insertSortedId(used_point_ids, id);
		return id;
	}

	VolumeCellId WorldData::createVolumeCell()
	{
		if(freed_volume_cell_ids.size() > 0)
		{
			const VolumeCellId id = freed_volume_cell_ids.pop();
			volume_cells[id] = VolumeCell();
			insertSortedId(used_volume_cell_ids, id);
			return id;
		}
		const VolumeCellId id = static_cast<VolumeCellId>(volume_cells.size());
		volume_cells.pushCtor();
		insertSortedId(used_volume_cell_ids, id);
		return id;
	}

	Chunk& WorldData::createChunk()
	{
		if(freed_chunk_ids.size() > 0)
		{
			const ChunkId id = freed_chunk_ids.pop();
			Chunk& chunk = chunks[id];
			chunk.position = vec3(0.0f);
			chunk.volume_cell_ids.clear();
			chunk.point_component_slot_ids.clear();
			chunk.graph_component_slot_ids.clear();
			chunk.invalidateMesh();
			insertSortedId(used_chunk_ids, id);
			return chunk;
		}
		Chunk& chunk = chunks.pushCtor();
		const ChunkId id = static_cast<ChunkId>(chunks.size() - 1);
		insertSortedId(used_chunk_ids, id);
		return chunk;
	}

	PointComponentSlotId WorldData::createPointComponentSlot(
		PointId point_id,
		PointComponentTypeId type_id,
		const void* init_data)
	{
		if(!isPointActive(point_id))
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

		PointComponentSlotId id;
		PointComponentSlot* slot_pointer;
		if(freed_point_component_slot_ids.size() > 0)
		{
			id = freed_point_component_slot_ids.pop();
			slot_pointer = &point_components[id];
		}
		else
		{
			id = static_cast<PointComponentSlotId>(point_components.size());
			slot_pointer = &point_components.pushCtor();
		}
		PointComponentSlot& slot = *slot_pointer;
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
		insertSortedId(used_point_component_slot_ids, id);
		return id;
	}

	GraphComponentSlotId WorldData::createGraphComponentSlot(
		ChunkId chunk_id,
		PointId point_a,
		PointId point_b,
		GraphComponentTypeId type_id,
		const void* init_data)
	{
		if(!isChunkActive(chunk_id))
		{
			danikk_engine::formatFatalError("Graph component references invalid chunk %", chunk_id);
		}
		if(!isPointActive(point_a) || !isPointActive(point_b))
		{
			danikk_engine::formatFatalError("Graph component references invalid endpoint points % and %", point_a, point_b);
		}

		const GraphComponentType& type = getGraphComponentType(type_id);
		if(type_id == GraphComponentTypeId::none || type.construct == nullptr || type.data_size == 0)
		{
			danikk_engine::formatFatalError("Graph component type % cannot be constructed", static_cast<uint32>(type_id));
		}
		if(type.data_alignment > alignof(std::max_align_t))
		{
			danikk_engine::formatFatalError("Graph component type % requires unsupported alignment", static_cast<uint32>(type_id));
		}

		GraphComponentSlotId id;
		GraphComponentSlot* slot_pointer;
		if(freed_graph_component_slot_ids.size() > 0)
		{
			id = freed_graph_component_slot_ids.pop();
			slot_pointer = &graph_components[id];
		}
		else
		{
			id = static_cast<GraphComponentSlotId>(graph_components.size());
			slot_pointer = &graph_components.pushCtor();
		}

		GraphComponentSlot& slot = *slot_pointer;
		slot.point_a = point_a;
		slot.point_b = point_b;
		slot.type_id = type_id;
		slot.uses_external_data = !type.inline_relocatable
			|| type.data_size > GraphComponentSlot::inline_data_capacity;
		if(slot.uses_external_data)
		{
			slot.external_data = (danikk_framework::malloc)(type.data_size);
			if(slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Cannot allocate % bytes for graph component type %", type.data_size, static_cast<uint32>(type_id));
			}
		}
		type.construct(slot.getData(), init_data);
		insertSortedId(used_graph_component_slot_ids, id);
		getChunk(chunk_id).addGraphComponentSlot(id);
		return id;
	}

	void WorldData::destroyPoint(PointId id)
	{
		if(!isPointActive(id)) return;

		for(size_t slot_index = 0; slot_index < used_point_component_slot_ids.size();)
		{
			const PointComponentSlotId slot_id = used_point_component_slot_ids[slot_index];
			if(point_components[slot_id].point_id == id)
			{
				destroyPointComponentSlot(slot_id);
			}
			else slot_index++;
		}

		for(size_t cell_index = 0; cell_index < used_volume_cell_ids.size();)
		{
			const VolumeCellId cell_id = used_volume_cell_ids[cell_index];
			const VolumeCell& cell = volume_cells[cell_id];
			const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
			for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
			{
				if(cell.points[point_index] == id)
				{
					destroyVolumeCell(cell_id);
					break;
				}
			}
			if(cell_index < used_volume_cell_ids.size() && used_volume_cell_ids[cell_index] == cell_id) cell_index++;
		}

		for(size_t graph_index = 0; graph_index < used_graph_component_slot_ids.size();)
		{
			const GraphComponentSlotId graph_id = used_graph_component_slot_ids[graph_index];
			const GraphComponentSlot& graph = graph_components[graph_id];
			if(graph.point_a == id || graph.point_b == id) destroyGraphComponentSlot(graph_id);
			else graph_index++;
		}

		eraseSortedId(used_point_ids, id);
		freed_point_ids.push(id);
	}

	void WorldData::destroyVolumeCell(VolumeCellId id)
	{
		if(!isVolumeCellActive(id)) return;

		for(size_t chunk_index = 0; chunk_index < used_chunk_ids.size(); chunk_index++)
		{
			const ChunkId chunk_id = used_chunk_ids[chunk_index];
			Chunk& chunk = chunks[chunk_id];
			for(size_t cell_index = 0; cell_index < chunk.volume_cell_ids.size();)
			{
				if(chunk.volume_cell_ids[cell_index] == id)
				{
					for(size_t shift_index = cell_index; shift_index + 1 < chunk.volume_cell_ids.size(); shift_index++)
					{
						chunk.volume_cell_ids[shift_index] = chunk.volume_cell_ids[shift_index + 1];
					}
					chunk.volume_cell_ids.resize(chunk.volume_cell_ids.size() - 1);
					chunk.invalidateMesh();
				}
				else
				{
					cell_index++;
				}
			}
		}

		eraseSortedId(used_volume_cell_ids, id);
		freed_volume_cell_ids.push(id);
	}

	void WorldData::destroyChunk(ChunkId id)
	{
		if(!isChunkActive(id)) return;

		Chunk& chunk = chunks[id];
		while(chunk.point_component_slot_ids.size() > 0)
		{
			destroyPointComponentSlot(chunk.point_component_slot_ids[chunk.point_component_slot_ids.size() - 1]);
		}
		while(chunk.graph_component_slot_ids.size() > 0)
		{
			destroyGraphComponentSlot(chunk.graph_component_slot_ids[chunk.graph_component_slot_ids.size() - 1]);
		}
		while(chunk.volume_cell_ids.size() > 0)
		{
			destroyVolumeCell(chunk.volume_cell_ids[chunk.volume_cell_ids.size() - 1]);
		}
		chunk.destroyMesh();
		chunk.position = vec3(0.0f);
		chunk.mesh_dirty = false;
		eraseSortedId(used_chunk_ids, id);
		freed_chunk_ids.push(id);
	}

	void WorldData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;

		for(size_t chunk_index = 0; chunk_index < used_chunk_ids.size(); chunk_index++)
		{
			const ChunkId chunk_id = used_chunk_ids[chunk_index];
			Chunk& chunk = chunks[chunk_id];
			for(size_t slot_index = 0; slot_index < chunk.point_component_slot_ids.size();)
			{
				if(chunk.point_component_slot_ids[slot_index] == id)
				{
					for(size_t shift_index = slot_index; shift_index + 1 < chunk.point_component_slot_ids.size(); shift_index++)
					{
						chunk.point_component_slot_ids[shift_index] = chunk.point_component_slot_ids[shift_index + 1];
					}
					chunk.point_component_slot_ids.resize(chunk.point_component_slot_ids.size() - 1);
				}
				else
				{
					slot_index++;
				}
			}
		}

		PointComponentSlot& slot = point_components[id];
		const PointComponentType& type = getPointComponentType(slot.type_id);
		if(type.destroy == nullptr)
		{
			danikk_engine::formatFatalError("Point component type % has no destroy function", static_cast<uint32>(slot.type_id));
		}
		if(slot.uses_external_data && slot.external_data == nullptr)
		{
			danikk_engine::formatFatalError("Point component slot % has no external data", id);
		}
		type.destroy(slot.getData());
		if(slot.uses_external_data)
		{
			free(slot.external_data);
			slot.external_data = nullptr;
		}
		slot.point_id = 0;
		slot.type_id = PointComponentTypeId::none;
		slot.uses_external_data = false;
		eraseSortedId(used_point_component_slot_ids, id);
		freed_point_component_slot_ids.push(id);
	}

	void WorldData::destroyGraphComponentSlot(GraphComponentSlotId id)
	{
		if(!isGraphComponentSlotActive(id)) return;

		for(ChunkId chunk_id : used_chunk_ids)
		{
			Chunk& chunk = chunks[chunk_id];
			for(size_t slot_index = 0; slot_index < chunk.graph_component_slot_ids.size();)
			{
				if(chunk.graph_component_slot_ids[slot_index] == id)
				{
					for(size_t shift_index = slot_index; shift_index + 1 < chunk.graph_component_slot_ids.size(); shift_index++)
					{
						chunk.graph_component_slot_ids[shift_index] = chunk.graph_component_slot_ids[shift_index + 1];
					}
					chunk.graph_component_slot_ids.resize(chunk.graph_component_slot_ids.size() - 1);
				}
				else slot_index++;
			}
		}

		GraphComponentSlot& slot = graph_components[id];
		const GraphComponentType& type = getGraphComponentType(slot.type_id);
		if(type.destroy == nullptr)
		{
			danikk_engine::formatFatalError("Graph component type % has no destroy function", static_cast<uint32>(slot.type_id));
		}
		if(slot.uses_external_data && slot.external_data == nullptr)
		{
			danikk_engine::formatFatalError("Graph component slot % has no external data", id);
		}
		type.destroy(slot.getData());
		if(slot.uses_external_data)
		{
			free(slot.external_data);
			slot.external_data = nullptr;
		}
		slot.point_a = 0;
		slot.point_b = 0;
		slot.type_id = GraphComponentTypeId::none;
		slot.uses_external_data = false;
		eraseSortedId(used_graph_component_slot_ids, id);
		freed_graph_component_slot_ids.push(id);
	}

	bool WorldData::isPointActive(PointId id) const
	{
		const size_t index = findSortedId(used_point_ids, id);
		return index < used_point_ids.size() && used_point_ids[index] == id;
	}

	bool WorldData::isVolumeCellActive(VolumeCellId id) const
	{
		const size_t index = findSortedId(used_volume_cell_ids, id);
		return index < used_volume_cell_ids.size() && used_volume_cell_ids[index] == id;
	}

	bool WorldData::isChunkActive(ChunkId id) const
	{
		const size_t index = findSortedId(used_chunk_ids, id);
		return index < used_chunk_ids.size() && used_chunk_ids[index] == id;
	}

	bool WorldData::isPointComponentSlotActive(PointComponentSlotId id) const
	{
		const size_t index = findSortedId(used_point_component_slot_ids, id);
		return index < used_point_component_slot_ids.size() && used_point_component_slot_ids[index] == id;
	}

	bool WorldData::isGraphComponentSlotActive(GraphComponentSlotId id) const
	{
		const size_t index = findSortedId(used_graph_component_slot_ids, id);
		return index < used_graph_component_slot_ids.size() && used_graph_component_slot_ids[index] == id;
	}

	const DynamicArray<PointId>& WorldData::getUsedPointIds() const { return used_point_ids; }
	const DynamicArray<VolumeCellId>& WorldData::getUsedVolumeCellIds() const { return used_volume_cell_ids; }
	const DynamicArray<ChunkId>& WorldData::getUsedChunkIds() const { return used_chunk_ids; }
	const DynamicArray<PointComponentSlotId>& WorldData::getUsedPointComponentSlotIds() const { return used_point_component_slot_ids; }
	const DynamicArray<GraphComponentSlotId>& WorldData::getUsedGraphComponentSlotIds() const { return used_graph_component_slot_ids; }

	Point& WorldData::getPoint(PointId id)
	{
		if(!isPointActive(id)) danikk_engine::formatFatalError("Invalid point ID %", id);
		return points[id];
	}

	const Point& WorldData::getPoint(PointId id) const
	{
		if(!isPointActive(id)) danikk_engine::formatFatalError("Invalid point ID %", id);
		return points[id];
	}

	VolumeCell& WorldData::getVolumeCell(VolumeCellId id)
	{
		if(!isVolumeCellActive(id)) danikk_engine::formatFatalError("Invalid volume cell ID %", id);
		return volume_cells[id];
	}

	const VolumeCell& WorldData::getVolumeCell(VolumeCellId id) const
	{
		if(!isVolumeCellActive(id)) danikk_engine::formatFatalError("Invalid volume cell ID %", id);
		return volume_cells[id];
	}

	Chunk& WorldData::getChunk(ChunkId id)
	{
		if(!isChunkActive(id)) danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return chunks[id];
	}

	const Chunk& WorldData::getChunk(ChunkId id) const
	{
		if(!isChunkActive(id)) danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return chunks[id];
	}

	PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) danikk_engine::formatFatalError("Invalid point component slot ID %", id);
		return point_components[id];
	}

	const PointComponentSlot& WorldData::getPointComponentSlot(PointComponentSlotId id) const
	{
		if(!isPointComponentSlotActive(id)) danikk_engine::formatFatalError("Invalid point component slot ID %", id);
		return point_components[id];
	}

	GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id)
	{
		if(!isGraphComponentSlotActive(id)) danikk_engine::formatFatalError("Invalid graph component slot ID %", id);
		return graph_components[id];
	}

	const GraphComponentSlot& WorldData::getGraphComponentSlot(GraphComponentSlotId id) const
	{
		if(!isGraphComponentSlotActive(id)) danikk_engine::formatFatalError("Invalid graph component slot ID %", id);
		return graph_components[id];
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

	size_t WorldData::getGraphComponentSlotCount() const
	{
		return graph_components.size();
	}

	void WorldData::clear()
	{
		while(used_point_component_slot_ids.size() > 0)
		{
			destroyPointComponentSlot(used_point_component_slot_ids[used_point_component_slot_ids.size() - 1]);
		}
		while(used_graph_component_slot_ids.size() > 0)
		{
			destroyGraphComponentSlot(used_graph_component_slot_ids[used_graph_component_slot_ids.size() - 1]);
		}
		point_components.clear();
		graph_components.clear();
		chunks.clear();
		points.clear();
		volume_cells.clear();
		used_point_ids.clear();
		used_volume_cell_ids.clear();
		used_chunk_ids.clear();
		used_point_component_slot_ids.clear();
		used_graph_component_slot_ids.clear();
		freed_point_ids.clear();
		freed_volume_cell_ids.clear();
		freed_chunk_ids.clear();
		freed_point_component_slot_ids.clear();
		freed_graph_component_slot_ids.clear();
	}

	void WorldData::release()
	{
		clear();
		chunks = DynamicArray<Chunk>();
		point_components = DynamicArray<PointComponentSlot>();
		graph_components = DynamicArray<GraphComponentSlot>();
		points = DynamicArray<Point>();
		volume_cells = DynamicArray<VolumeCell>();
		used_point_ids = DynamicArray<PointId>();
		used_volume_cell_ids = DynamicArray<VolumeCellId>();
		used_chunk_ids = DynamicArray<ChunkId>();
		used_point_component_slot_ids = DynamicArray<PointComponentSlotId>();
		used_graph_component_slot_ids = DynamicArray<GraphComponentSlotId>();
		freed_point_ids = DynamicArray<PointId>();
		freed_volume_cell_ids = DynamicArray<VolumeCellId>();
		freed_chunk_ids = DynamicArray<ChunkId>();
		freed_point_component_slot_ids = DynamicArray<PointComponentSlotId>();
		freed_graph_component_slot_ids = DynamicArray<GraphComponentSlotId>();
	}
}
