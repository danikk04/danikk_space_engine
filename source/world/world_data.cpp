#include <danikk_space_engine/world/world_data.h>

#include <cstdlib>

namespace danikk_space_engine
{
	PointId WorldData::createPoint()
	{
		return points.create();
	}

	VolumeCellId WorldData::createVolumeCell()
	{
		return volume_cells.create();
	}

	GroundTriangleId WorldData::createGroundTriangle()
	{
		return ground_triangles.create();
	}

	WorldBodyId WorldData::createWorldBody()
	{
		return world_bodies.create();
	}

	Chunk& WorldData::createChunk(WorldBodyId world_body_id)
	{
		if(!isWorldBodyActive(world_body_id))
		{
			danikk_engine::formatFatalError("Chunk references invalid world body %", world_body_id);
		}
		const ChunkId id = chunks.create();
		Chunk& chunk = chunks[id];
		chunk.chunk_id = id;
		chunk.world_body_id = world_body_id;
		chunk.grid_coordinate = glm::ivec3(0);
		getWorldBody(world_body_id).addChunk(id);
		return chunk;
	}

	void WorldData::addPointReferenceToChunk(ChunkId chunk_id, PointId point_id)
	{
		if(!isChunkActive(chunk_id) || !isPointActive(point_id))
		{
			danikk_engine::formatFatalError("Cannot attach point % to invalid chunk %", point_id, chunk_id);
		}

		Chunk& chunk = getChunk(chunk_id);
		const size_t point_index = findWorldResourceId(chunk.point_ids, point_id);
		if(point_index < chunk.point_ids.size() && chunk.point_ids[point_index] == point_id) return;

		Point& point = getPoint(point_id);
		if(point.chunk_reference_count >= 100)
		{
			danikk_engine::formatFatalError("Point % exceeds the maximum of 100 chunk references", point_id);
		}
		insertWorldResourceId(chunk.point_ids, point_id);
		point.chunk_reference_count++;
	}

	void WorldData::addVolumeCellToChunk(ChunkId chunk_id, VolumeCellId cell_id)
	{
		if(!isChunkActive(chunk_id) || !isVolumeCellActive(cell_id))
		{
			danikk_engine::formatFatalError("Cannot attach volume cell % to invalid chunk %", cell_id, chunk_id);
		}
		Chunk& chunk = getChunk(chunk_id);
		for(VolumeCellId existing_cell_id : chunk.volume_cell_ids)
		{
			if(existing_cell_id == cell_id)
			{
				danikk_engine::formatFatalError("Volume cell % is already attached to chunk %", cell_id, chunk_id);
			}
		}

		const VolumeCell& cell = getVolumeCell(cell_id);
		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
		for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
		{
			addPointReferenceToChunk(chunk_id, cell.points[point_index]);
		}
		chunk.volume_cell_ids.push(cell_id);
		chunk.invalidateMesh();
	}

	void WorldData::addGroundTriangleToChunk(ChunkId chunk_id, GroundTriangleId triangle_id)
	{
		if(!isChunkActive(chunk_id) || !isGroundTriangleActive(triangle_id))
		{
			danikk_engine::formatFatalError("Cannot attach ground triangle % to invalid chunk %", triangle_id, chunk_id);
		}
		Chunk& chunk = getChunk(chunk_id);
		for(GroundTriangleId existing_id : chunk.ground_triangle_ids)
		{
			if(existing_id == triangle_id)
			{
				danikk_engine::formatFatalError("Ground triangle % is already attached to chunk %", triangle_id, chunk_id);
			}
		}
		const GroundTriangle& triangle = getGroundTriangle(triangle_id);
		for(PointId point_id : triangle.points) addPointReferenceToChunk(chunk_id, point_id);
		chunk.ground_triangle_ids.push(triangle_id);
		chunk.invalidateMesh();
	}

	void WorldData::addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id)
	{
		if(!isChunkActive(chunk_id) || !isPointComponentSlotActive(slot_id))
		{
			danikk_engine::formatFatalError("Cannot attach point component % to invalid chunk %", slot_id, chunk_id);
		}
		Chunk& chunk = getChunk(chunk_id);
		for(PointComponentSlotId existing_slot_id : chunk.point_component_slot_ids)
		{
			if(existing_slot_id == slot_id)
			{
				danikk_engine::formatFatalError("Point component % is already attached to chunk %", slot_id, chunk_id);
			}
		}
		addPointReferenceToChunk(chunk_id, getPointComponentSlot(slot_id).point_id);
		chunk.addPointComponentSlot(slot_id);
		getPointComponentSlot(slot_id).chunk_id = chunk_id;
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

		const PointComponentSlotId id = point_components.create();
		PointComponentSlot& slot = point_components[id];
		slot.point_id = point_id;
		slot.chunk_id = static_cast<uint32>(-1);
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

		const GraphComponentSlotId id = graph_components.create();
		GraphComponentSlot& slot = graph_components[id];
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
		Chunk& chunk = getChunk(chunk_id);
		addPointReferenceToChunk(chunk_id, point_a);
		addPointReferenceToChunk(chunk_id, point_b);
		chunk.addGraphComponentSlot(id);
		return id;
	}

	void WorldData::destroyPoint(PointId id)
	{
		if(!isPointActive(id)) return;

		// Explicit destruction invalidates all chunk references before cascading to cells and components.
		for(ChunkId chunk_id : chunks.usedIds())
		{
			Chunk& chunk = chunks[chunk_id];
			for(size_t point_index = 0; point_index < chunk.point_ids.size();)
			{
				if(chunk.point_ids[point_index] != id)
				{
					point_index++;
					continue;
				}
				for(size_t shift_index = point_index; shift_index + 1 < chunk.point_ids.size(); shift_index++)
				{
					chunk.point_ids[shift_index] = chunk.point_ids[shift_index + 1];
				}
				chunk.point_ids.resize(chunk.point_ids.size() - 1);
			}
		}
		points[id].chunk_reference_count = 0;

		for(size_t slot_index = 0; slot_index < point_components.usedIds().size();)
		{
			const PointComponentSlotId slot_id = point_components.usedIds()[slot_index];
			if(point_components[slot_id].point_id == id)
			{
				destroyPointComponentSlot(slot_id);
			}
			else slot_index++;
		}

		for(size_t cell_index = 0; cell_index < volume_cells.usedIds().size();)
		{
			const VolumeCellId cell_id = volume_cells.usedIds()[cell_index];
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
			if(cell_index < volume_cells.usedIds().size() && volume_cells.usedIds()[cell_index] == cell_id) cell_index++;
		}

		for(size_t triangle_index = 0; triangle_index < ground_triangles.usedIds().size();)
		{
			const GroundTriangleId triangle_id = ground_triangles.usedIds()[triangle_index];
			const GroundTriangle& triangle = ground_triangles[triangle_id];
			if(triangle.points[0] == id || triangle.points[1] == id || triangle.points[2] == id)
			{
				destroyGroundTriangle(triangle_id);
			}
			else triangle_index++;
		}

		for(size_t graph_index = 0; graph_index < graph_components.usedIds().size();)
		{
			const GraphComponentSlotId graph_id = graph_components.usedIds()[graph_index];
			const GraphComponentSlot& graph = graph_components[graph_id];
			if(graph.point_a == id || graph.point_b == id) destroyGraphComponentSlot(graph_id);
			else graph_index++;
		}

		points.free(id);
	}

	void WorldData::destroyVolumeCell(VolumeCellId id)
	{
		if(!isVolumeCellActive(id)) return;

		for(size_t chunk_index = 0; chunk_index < chunks.usedIds().size(); chunk_index++)
		{
			const ChunkId chunk_id = chunks.usedIds()[chunk_index];
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

		volume_cells.free(id);
	}

	void WorldData::destroyGroundTriangle(GroundTriangleId id)
	{
		if(!isGroundTriangleActive(id)) return;
		for(ChunkId chunk_id : chunks.usedIds())
		{
			Chunk& chunk = chunks[chunk_id];
			for(size_t index = 0; index < chunk.ground_triangle_ids.size();)
			{
				if(chunk.ground_triangle_ids[index] == id)
				{
					for(size_t shift = index; shift + 1 < chunk.ground_triangle_ids.size(); shift++)
					{
						chunk.ground_triangle_ids[shift] = chunk.ground_triangle_ids[shift + 1];
					}
					chunk.ground_triangle_ids.resize(chunk.ground_triangle_ids.size() - 1);
					chunk.invalidateMesh();
				}
				else index++;
			}
		}
		ground_triangles.free(id);
	}

	void WorldData::destroyChunk(ChunkId id)
	{
		if(!isChunkActive(id)) return;

		Chunk& chunk = chunks[id];
		for(WorldBodyId body_id : world_bodies.usedIds()) world_bodies[body_id].removeChunk(id);
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
		while(chunk.ground_triangle_ids.size() > 0)
		{
			destroyGroundTriangle(chunk.ground_triangle_ids[chunk.ground_triangle_ids.size() - 1]);
		}
		chunk.destroyMesh();
		while(chunk.point_ids.size() > 0)
		{
			const PointId point_id = chunk.point_ids[chunk.point_ids.size() - 1];
			chunk.point_ids.resize(chunk.point_ids.size() - 1);
			if(!isPointActive(point_id)) continue;
			Point& point = getPoint(point_id);
			if(point.chunk_reference_count == 0)
			{
				danikk_engine::formatFatalError("Point % has no chunk references while releasing chunk %", point_id, id);
			}
			point.chunk_reference_count--;
			if(point.chunk_reference_count == 0) destroyPoint(point_id);
		}
		chunk.world_body_id = 0;
		chunk.grid_coordinate = glm::ivec3(0);
		chunk.position = glm::dvec3(0.0);
		chunk.mesh_dirty = false;
		chunks.free(id);
	}

	void WorldData::destroyWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) return;
		WorldBody& body = world_bodies[id];
		while(body.chunk_ids.size() > 0) destroyChunk(body.chunk_ids[body.chunk_ids.size() - 1]);
		world_bodies.free(id);
	}

	void WorldData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;

		for(size_t chunk_index = 0; chunk_index < chunks.usedIds().size(); chunk_index++)
		{
			const ChunkId chunk_id = chunks.usedIds()[chunk_index];
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
		slot.chunk_id = static_cast<uint32>(-1);
		slot.type_id = PointComponentTypeId::none;
		slot.uses_external_data = false;
		point_components.free(id);
	}

	void WorldData::destroyGraphComponentSlot(GraphComponentSlotId id)
	{
		if(!isGraphComponentSlotActive(id)) return;

		for(ChunkId chunk_id : chunks.usedIds())
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
		graph_components.free(id);
	}

	bool WorldData::isPointActive(PointId id) const
	{
		return points.isActive(id);
	}

	bool WorldData::isVolumeCellActive(VolumeCellId id) const
	{
		return volume_cells.isActive(id);
	}

	bool WorldData::isGroundTriangleActive(GroundTriangleId id) const
	{
		return ground_triangles.isActive(id);
	}

	bool WorldData::isChunkActive(ChunkId id) const
	{
		return chunks.isActive(id);
	}

	bool WorldData::isWorldBodyActive(WorldBodyId id) const
	{
		return world_bodies.isActive(id);
	}

	bool WorldData::isPointComponentSlotActive(PointComponentSlotId id) const
	{
		return point_components.isActive(id);
	}

	bool WorldData::isGraphComponentSlotActive(GraphComponentSlotId id) const
	{
		return graph_components.isActive(id);
	}

	const DynamicArray<PointId>& WorldData::getUsedPointIds() const { return points.usedIds(); }
	const DynamicArray<VolumeCellId>& WorldData::getUsedVolumeCellIds() const { return volume_cells.usedIds(); }
	const DynamicArray<GroundTriangleId>& WorldData::getUsedGroundTriangleIds() const { return ground_triangles.usedIds(); }
	const DynamicArray<ChunkId>& WorldData::getUsedChunkIds() const { return chunks.usedIds(); }
	const DynamicArray<WorldBodyId>& WorldData::getUsedWorldBodyIds() const { return world_bodies.usedIds(); }
	const DynamicArray<PointComponentSlotId>& WorldData::getUsedPointComponentSlotIds() const { return point_components.usedIds(); }
	const DynamicArray<GraphComponentSlotId>& WorldData::getUsedGraphComponentSlotIds() const { return graph_components.usedIds(); }

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

	GroundTriangle& WorldData::getGroundTriangle(GroundTriangleId id)
	{
		if(!isGroundTriangleActive(id)) danikk_engine::formatFatalError("Invalid ground triangle ID %", id);
		return ground_triangles[id];
	}

	const GroundTriangle& WorldData::getGroundTriangle(GroundTriangleId id) const
	{
		if(!isGroundTriangleActive(id)) danikk_engine::formatFatalError("Invalid ground triangle ID %", id);
		return ground_triangles[id];
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

	WorldBody& WorldData::getWorldBody(WorldBodyId id)
	{
		if(!isWorldBodyActive(id)) danikk_engine::formatFatalError("Invalid world body ID %", id);
		return world_bodies[id];
	}

	const WorldBody& WorldData::getWorldBody(WorldBodyId id) const
	{
		if(!isWorldBodyActive(id)) danikk_engine::formatFatalError("Invalid world body ID %", id);
		return world_bodies[id];
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

	size_t WorldData::getGroundTriangleCount() const
	{
		return ground_triangles.size();
	}

	size_t WorldData::getChunkCount() const
	{
		return chunks.size();
	}

	size_t WorldData::getWorldBodyCount() const
	{
		return world_bodies.size();
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
		while(point_components.usedIds().size() > 0)
		{
			destroyPointComponentSlot(point_components.usedIds()[point_components.usedIds().size() - 1]);
		}
		while(graph_components.usedIds().size() > 0)
		{
			destroyGraphComponentSlot(graph_components.usedIds()[graph_components.usedIds().size() - 1]);
		}
		point_components.clear();
		graph_components.clear();
		chunks.clear();
		world_bodies.clear();
		points.clear();
		volume_cells.clear();
		ground_triangles.clear();
	}

	void WorldData::release()
	{
		clear();
		points.releaseMemory();
		volume_cells.releaseMemory();
		ground_triangles.releaseMemory();
		chunks.releaseMemory();
		world_bodies.releaseMemory();
		point_components.releaseMemory();
		graph_components.releaseMemory();
	}
}
