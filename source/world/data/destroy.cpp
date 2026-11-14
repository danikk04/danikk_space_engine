#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/data.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{
		template<typename id_t>
		bool eraseId(DynamicArray<id_t>& ids, id_t id)
		{
			bool erased = false;
			for(size_t index = 0; index < ids.size();)
			{
				if(ids[index] != id)
				{
					index++;
					continue;
				}
				for(size_t shift = index; shift + 1 < ids.size(); shift++) ids[shift] = ids[shift + 1];
				ids.resize(ids.size() - 1);
				erased = true;
			}
			return erased;
		}

		template<typename array_t, typename id_t>
		void eraseIdFromChunks(
			WorldData& world_data,
			const DynamicArray<ChunkId>& chunk_ids,
			id_t id,
			array_t Chunk::* chunk_array,
			bool invalidate_mesh = false)
		{
			for(ChunkId chunk_id : chunk_ids)
			{
				Chunk& chunk = world_data.getChunk(chunk_id);
				if(eraseId(chunk.*chunk_array, id) && invalidate_mesh) chunk.invalidateMesh();
			}
		}

		template<typename pool_t, typename predicate_t, typename destroy_t>
		void destroyMatchingResources(pool_t& pool, predicate_t matches, destroy_t destroy)
		{
			for(size_t index = 0; index < pool.usedIds().size();)
			{
				const auto id = pool.usedIds()[index];
				if(matches(pool[id])) destroy(id);
				else index++;
			}
		}

		template<typename list_t, typename destroy_t>
		void destroyLastUntilEmpty(const list_t& ids, destroy_t destroy)
		{
			while(ids.size() > 0) destroy(ids[ids.size() - 1]);
		}

		template<typename... pool_t>
		void clearPools(pool_t&... pools)
		{
			(pools.clear(), ...);
		}

		template<typename... pool_t>
		void releasePools(pool_t&... pools)
		{
			(pools.releaseMemory(), ...);
		}

		template<typename slot_t, typename component_type_t, typename id_t>
		void destroyComponentPayload(
			slot_t& slot,
			const component_type_t& type,
			id_t id,
			const char* component_name)
		{
			if(type.destroy == nullptr)
			{
				danikk_engine::formatFatalError("% component type % has no destroy function", component_name, static_cast<uint32>(slot.type_id));
			}
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("% component slot % has no external data", component_name, id);
			}
			type.destroy(slot.getData());
			if(slot.uses_external_data)
			{
				free(slot.external_data);
				slot.external_data = nullptr;
			}
		}
	}

	// Removes all chunk references first, then walks the active pools for resources
	// that use the point. Each matching resource is destroyed through its normal
	// cleanup path; the scan keeps its index when an ID is erased because the next
	// active ID shifts into that position. The point slot is freed after dependents.
	void WorldData::destroyPoint(PointId id)
	{
		Watcher watcher("WorldData::destroyPoint");
		if(!isPointActive(id)) return;

		eraseIdFromChunks(*this, chunks.usedIds(), id, &Chunk::point_ids);
		points[id].chunk_reference_count = 0;

		destroyMatchingResources(point_components,
			[id](const PointComponentSlot& slot) { return slot.point_id == id; },
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		destroyMatchingResources(volume_cells,
			[id](const VolumeCell& cell)
			{
				const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
				for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
				{
					if(cell.points[point_index] == id) return true;
				}
				return false;
			},
			[this](VolumeCellId cell_id) { destroyVolumeCell(cell_id); });
		destroyMatchingResources(ground_triangles,
			[id](const GroundTriangle& triangle)
			{
				return triangle.points[0] == id || triangle.points[1] == id || triangle.points[2] == id;
			},
			[this](GroundTriangleId triangle_id) { destroyGroundTriangle(triangle_id); });
		destroyMatchingResources(graph_components,
			[id](const GraphComponentSlot& graph) { return graph.point_a == id || graph.point_b == id; },
			[this](GraphComponentSlotId graph_id) { destroyGraphComponentSlot(graph_id); });

		points.free(id);
	}

	void WorldData::destroyVolumeCell(VolumeCellId id)
	{
		if(!isVolumeCellActive(id)) return;
		VolumeCell& cell = volume_cells[id];
		if(cell.chunk_id != VolumeCell::no_chunk)
		{
			if(!isChunkActive(cell.chunk_id))
			{
				danikk_engine::formatFatalError("Volume cell % references invalid chunk %", id, cell.chunk_id);
			}
			Chunk& chunk = getChunk(cell.chunk_id);
			const size_t index = cell.index_in_chunk;
			if(index >= chunk.volume_cell_ids.size() || chunk.volume_cell_ids[index] != id)
			{
				danikk_engine::formatFatalError("Volume cell % has an invalid chunk index", id);
			}
			const size_t last_index = chunk.volume_cell_ids.size() - 1;
			if(index != last_index)
			{
				const VolumeCellId moved_id = chunk.volume_cell_ids[last_index];
				chunk.volume_cell_ids[index] = moved_id;
				volume_cells[moved_id].index_in_chunk = index;
			}
			chunk.volume_cell_ids.resize(last_index);
			chunk.invalidateMesh();
		}
		volume_cells.free(id);
	}

	void WorldData::destroyGroundTriangle(GroundTriangleId id)
	{
		Watcher watcher("WorldData::destroyGroundTriangle");
		if(!isGroundTriangleActive(id)) return;
		GroundTriangle& triangle = ground_triangles[id];
		if(triangle.chunk_id != GroundTriangle::no_chunk)
		{
			if(!isChunkActive(triangle.chunk_id))
			{
				danikk_engine::formatFatalError("Ground triangle % references invalid chunk %", id, triangle.chunk_id);
			}
			Chunk& chunk = getChunk(triangle.chunk_id);
			const size_t index = triangle.index_in_chunk;
			if(index >= chunk.ground_triangle_ids.size() || chunk.ground_triangle_ids[index] != id)
			{
				danikk_engine::formatFatalError("Ground triangle % has an invalid chunk index", id);
			}
			const size_t last_index = chunk.ground_triangle_ids.size() - 1;
			if(index != last_index)
			{
				const GroundTriangleId moved_id = chunk.ground_triangle_ids[last_index];
				chunk.ground_triangle_ids[index] = moved_id;
				ground_triangles[moved_id].index_in_chunk = index;
			}
			chunk.ground_triangle_ids.resize(last_index);
			chunk.invalidateMesh();
		}
		ground_triangles.free(id);
	}

	// Detaches the chunk, destroys its components and geometry, releases its mesh,
	// then decrements ownership counts for its points. Points with no remaining
	// chunk owners are cascaded through destroyPoint() before the chunk slot is freed.
	void WorldData::destroyChunk(ChunkId id)
	{
		Watcher watcher("WorldData::destroyChunk");
		if(!isChunkActive(id)) return;

		Chunk& chunk = chunks[id];
		for(WorldBodyId body_id : world_bodies.usedIds()) world_bodies[body_id].removeChunk(id);
		destroyLastUntilEmpty(chunk.point_component_slot_ids,
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		destroyLastUntilEmpty(chunk.graph_component_slot_ids,
			[this](GraphComponentSlotId slot_id) { destroyGraphComponentSlot(slot_id); });
		destroyLastUntilEmpty(chunk.volume_cell_ids,
			[this](VolumeCellId cell_id) { destroyVolumeCell(cell_id); });
		destroyLastUntilEmpty(chunk.ground_triangle_ids,
			[this](GroundTriangleId triangle_id) { destroyGroundTriangle(triangle_id); });
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
			if(point.chunk_reference_count == 0)
			{
				// All components and geometry owned by this chunk were destroyed above.
				// A zero chunk reference count means no other chunk can still use this point.
				points.free(point_id);
			}
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
		while(body.component_slot_ids.size() > 0)
		{
			destroyWorldBodyComponentSlot(body.component_slot_ids[body.component_slot_ids.size() - 1]);
		}
		while(body.chunk_ids.size() > 0) destroyChunk(body.chunk_ids[body.chunk_ids.size() - 1]);
		world_bodies.free(id);
	}

	void WorldData::destroyWorldBodyComponentSlot(WorldBodyComponentSlotId id)
	{
		if(!isWorldBodyComponentSlotActive(id)) return;
		WorldBodyComponentSlot& slot = world_body_components[id];
		if(isWorldBodyActive(slot.world_body_id)) world_bodies[slot.world_body_id].removeComponentSlot(id);
		const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "World body");
		slot.dispose();
		world_body_components.free(id);
	}

	void WorldData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;
		eraseIdFromChunks(*this, chunks.usedIds(), id, &Chunk::point_component_slot_ids);

		PointComponentSlot& slot = point_components[id];
		const PointComponentType& type = getPointComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "Point");
		slot.dispose();
		point_components.free(id);
	}

	void WorldData::destroyGraphComponentSlot(GraphComponentSlotId id)
	{
		if(!isGraphComponentSlotActive(id)) return;
		eraseIdFromChunks(*this, chunks.usedIds(), id, &Chunk::graph_component_slot_ids);

		GraphComponentSlot& slot = graph_components[id];
		const GraphComponentType& type = getGraphComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "Graph");
		slot.dispose();
		graph_components.free(id);
	}

	void WorldData::clear()
	{
		destroyLastUntilEmpty(world_body_components.usedIds(),
			[this](WorldBodyComponentSlotId id) { destroyWorldBodyComponentSlot(id); });
		destroyLastUntilEmpty(point_components.usedIds(),
			[this](PointComponentSlotId id) { destroyPointComponentSlot(id); });
		destroyLastUntilEmpty(graph_components.usedIds(),
			[this](GraphComponentSlotId id) { destroyGraphComponentSlot(id); });
		clearPools(point_components, graph_components, world_body_components, chunks, world_bodies, points, volume_cells, ground_triangles);
	}

	void WorldData::release()
	{
		clear();
		releasePools(points, volume_cells, ground_triangles, chunks, world_bodies, world_body_components, point_components, graph_components);
	}
}
