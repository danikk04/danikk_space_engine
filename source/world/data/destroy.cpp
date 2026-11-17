#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/data.h>
#include <danikk_space_engine/component/point/build_part.h>

#include <cstdlib>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Removes every matching ID from a compact ownership list while preserving order.
		 * @tparam id_t ID value type stored in the owner list.
		 * @param ids List to clean up.
		 * @param id Resource ID to detach.
		 * @return True if at least one occurrence was removed.
		 */
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

		/**
		 * @brief Detaches a resource ID from each listed chunk.
		 * @tparam array_t Chunk member type holding resource IDs.
		 * @tparam id_t Resource ID type.
		 * @param world_data Data pools used to resolve chunks.
		 * @param chunk_ids Chunks whose ownership lists are searched.
		 * @param id Resource ID to remove.
		 * @param chunk_array Member pointer selecting the relevant ID list.
		 */
		template<typename array_t, typename id_t>
		void eraseIdFromChunks(
			WorldData& world_data,
			const DynamicArray<ChunkId>& chunk_ids,
			id_t id,
			array_t Chunk::* chunk_array)
		{
			for(ChunkId chunk_id : chunk_ids)
			{
				Chunk& chunk = world_data.getChunk(chunk_id);
				eraseId(chunk.*chunk_array, id);
			}
		}

		/**
		 * @brief Iterates active resources and invokes the destroy callback for predicate matches.
		 * @tparam pool_t Resource pool type.
		 * @tparam predicate_t Predicate accepting a resource reference.
		 * @tparam destroy_t Callback accepting the matching resource ID.
		 * @param pool Pool to traverse; traversal tolerates swap removal by rechecking the moved ID.
		 * @param matches Predicate deciding whether to destroy each resource.
		 * @param destroy Normal destruction callback for a matched ID.
		 */
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

		/**
		 * @brief Repeatedly destroys the last ID until an owner list is empty.
		 * @tparam list_t Array-like owner list exposing size() and indexed access.
		 * @tparam destroy_t Callback that removes the supplied resource ID from the list.
		 * @param ids List that is shortened by the callback.
		 * @param destroy Resource destruction callback.
		 */
		template<typename list_t, typename destroy_t>
		void destroyLastUntilEmpty(const list_t& ids, destroy_t destroy)
		{
			while(ids.size() > 0) destroy(ids[ids.size() - 1]);
		}

		/**
		 * @brief Calls clear() on every supplied pool, destroying elements but retaining array capacity.
		 * @tparam pool_t Pool types accepted by the fold expression.
		 * @param pools Pools to clear.
		 */
		template<typename... pool_t>
		void clearPools(pool_t&... pools)
		{
			(pools.clear(), ...);
		}

		/**
		 * @brief Calls releaseMemory() on every supplied pool to return backing arrays to the allocator.
		 * @tparam pool_t Pool types accepted by the fold expression.
		 * @param pools Pools whose storage is released.
		 */
		template<typename... pool_t>
		void releasePools(pool_t&... pools)
		{
			(pools.releaseMemory(), ...);
		}

		/**
		 * @brief Runs the registered component destructor and frees external payload storage when used.
		 * @tparam slot_t Point, graph, or world-body component slot type.
		 * @tparam component_type_t Matching callback descriptor type.
		 * @tparam id_t Slot ID type used for diagnostic messages.
		 * @param slot Component slot whose payload is being destroyed.
		 * @param type Descriptor supplying the destroy callback.
		 * @param id Slot ID reported if required storage/callbacks are missing.
		 * @param component_name Human-readable category used in fatal diagnostics.
		 */
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

	/**
	 * @brief Destroys all point components, triangles, and graph edges that reference the point.
	 *
	 * First detaches the point from every chunk, then destroys dependent resources
	 * through their regular cleanup paths, and finally recycles the point slot. The
	 * pool scan keeps its index after a removal because swap-remove may move another
	 * active ID into that position.
	 * @param id Point ID to destroy; inactive IDs are ignored.
	 */
	void WorldData::destroyPoint(PointId id)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(!isPointActive(id)) return;

		eraseIdFromChunks(*this, chunks.usedIds(), id, &Chunk::point_ids);
		points[id].referencing_chunk_count = 0;

		destroyMatchingResources(point_components,
			[id](const PointComponentSlot& slot) { return slot.point_id == id; },
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		destroyMatchingResources(graph_components,
			[id](const GraphComponentSlot& graph) { return graph.point_a == id || graph.point_b == id; },
			[this](GraphComponentSlotId graph_id) { destroyGraphComponentSlot(graph_id); });

		points.free(id);
	}

	/**
	 * @brief Detaches a chunk from bodies, destroys its components and geometry, then releases unreferenced points.
	 * Removes body ownership, destroys component slots and triangles, releases the GPU mesh, then
	 * decrements point reference counts. Any point whose count reaches zero is freed after this
	 * chunk's dependents have already been detached.
	 * @param id Chunk ID to destroy; inactive IDs are ignored.
	 */
	void WorldData::destroyChunk(ChunkId id)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		if(!isChunkActive(id)) return;

		Chunk& chunk = chunks[id];
		for(WorldBodyId body_id : world_bodies.usedIds()) world_bodies[body_id].removeChunk(id);
		destroyLastUntilEmpty(chunk.point_component_slot_ids,
			[this](PointComponentSlotId slot_id) { destroyPointComponentSlot(slot_id); });
		destroyLastUntilEmpty(chunk.graph_component_slot_ids,
			[this](GraphComponentSlotId slot_id) { destroyGraphComponentSlot(slot_id); });
		chunk.destroyMesh();

		while(chunk.point_ids.size() > 0)
		{
			const PointId point_id = chunk.point_ids[chunk.point_ids.size() - 1];
			chunk.point_ids.resize(chunk.point_ids.size() - 1);
			if(!isPointActive(point_id)) continue;
			Point& point = getPoint(point_id);
			if(point.referencing_chunk_count == 0)
			{
				danikk_engine::formatFatalError("Point % has no chunk references while releasing chunk %", point_id, id);
			}
			point.referencing_chunk_count--;
			if(point.referencing_chunk_count == 0)
			{
				/** @brief No other chunk can retain this point once its distinct-owner count reaches zero. */
				points.free(point_id);
			}
		}
		chunk.world_body_id = 0;
		chunk.grid_coordinate = glm::ivec3(0);
		chunk.position = glm::dvec3(0.0);
		chunk.planet_terrain = false;
		chunk.bounds_valid = false;
		chunk.material_ranges.clear();
		chunks.free(id);
	}

	/**
	 * @brief Destroys all components and chunks owned by the body before returning its slot to the pool.
	 * @param id WorldBody ID to destroy; inactive IDs are ignored.
	 */
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

	/**
	 * @brief Detaches a body component, destroys its payload, resets the slot, and recycles its ID.
	 * @param id Component slot ID to destroy; inactive IDs are ignored.
	 */
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

	/**
	 * @brief Detaches a point component from its owning chunk and releases its payload and slot.
	 * @param id Component slot ID to destroy; inactive IDs are ignored.
	 */
	void WorldData::destroyPointComponentSlot(PointComponentSlotId id)
	{
		if(!isPointComponentSlotActive(id)) return;
		const PointComponentSlot& destroyed_slot = point_components[id];
		if(destroyed_slot.type_id == PointComponentTypeId::build_part)
		{
			for(PointComponentSlotId other_id : point_components.usedIds())
				if(other_id != id && isPointComponentSlotActive(other_id))
				{
					PointComponentSlot& other_slot = point_components[other_id];
					if(other_slot.type_id != PointComponentTypeId::build_part) continue;
					BuildPartComponent& child = *static_cast<BuildPartComponent*>(other_slot.getData());
					if(child.parent_slot_id == id) child.parent_slot_id = no_build_part_parent;
				}
		}
		removePointComponentSlotFromChunk(id);

		PointComponentSlot& slot = point_components[id];
		const PointComponentType& type = getPointComponentType(slot.type_id);
		destroyComponentPayload(slot, type, id, "Point");
		slot.dispose();
		point_components.free(id);
	}

	/** @copydoc WorldData::removePointComponentSlotFromChunk */
	void WorldData::removePointComponentSlotFromChunk(PointComponentSlotId slot_id)
	{
		PointComponentSlot& slot = getPointComponentSlot(slot_id);
		if(slot.chunk_id == PointComponentSlot::no_chunk) return;

		getChunk(slot.chunk_id).removePointComponentSlot(slot_id, slot.type_id);
		slot.chunk_id = PointComponentSlot::no_chunk;
	}

	/**
	 * @brief Detaches an edge component from chunk lists and releases its payload and slot.
	 * @param id Component slot ID to destroy; inactive IDs are ignored.
	 */
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

	/**
	 * @brief Destroys active resources while retaining the allocated capacity of each pool.
	 */
	void WorldData::clear()
	{
		factions.clear();
		destroyLastUntilEmpty(world_body_components.usedIds(),
			[this](WorldBodyComponentSlotId id) { destroyWorldBodyComponentSlot(id); });
		destroyLastUntilEmpty(point_components.usedIds(),
			[this](PointComponentSlotId id) { destroyPointComponentSlot(id); });
		destroyLastUntilEmpty(graph_components.usedIds(),
			[this](GraphComponentSlotId id) { destroyGraphComponentSlot(id); });
		clearPools(point_components, graph_components, world_body_components, chunks, world_bodies, points);
	}

	/**
	 * @brief Destroys active resources and releases the backing memory of every pool.
	 */
	void WorldData::release()
	{
		clear();
		factions.release();
		releasePools(points, chunks, world_bodies, world_body_components, point_components, graph_components);
	}
}
