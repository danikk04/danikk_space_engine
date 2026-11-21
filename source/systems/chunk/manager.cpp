#include <danikk_space_engine/systems/chunk/manager.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Removes a point from a body's dense point list and repairs its moved entry index. */
		bool removePointFromBody(World& world, WorldBody& body, PointId point_id)
		{
			size_t index = static_cast<size_t>(-1);
			Point* point = world.point_data.isPointActive(point_id)
				? &world.point_data.getPoint(point_id) : nullptr;
			if(point != nullptr && point->world_body_id != static_cast<WorldBodyId>(-1)
				&& point->world_body_point_index < body.point_ids.size()
				&& body.point_ids[point->world_body_point_index] == point_id)
				index = point->world_body_point_index;
			else
				for(size_t candidate = 0; candidate < body.point_ids.size(); ++candidate)
					if(body.point_ids[candidate] == point_id) { index = candidate; break; }
			if(index == static_cast<size_t>(-1)) return false;
			const PointId moved_point_id = body.point_ids[body.point_ids.size() - 1];
			body.point_ids[index] = moved_point_id;
			body.point_ids.resize(body.point_ids.size() - 1);
			if(moved_point_id != point_id && world.point_data.isPointActive(moved_point_id))
				world.point_data.getPoint(moved_point_id).world_body_point_index = index;
			if(point != nullptr)
			{
				point->world_body_id = static_cast<WorldBodyId>(-1);
				point->world_body_point_index = static_cast<size_t>(-1);
			}
			return true;
		}
	}

	void ChunkSystem::init()
	{
		danikk_framework::formatLogInfo("Init ChunkSystem");
		clear();
	}
	void ChunkSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown ChunkSystem");
		clear();
	}
	/** @brief Returns a mutable chunk after validating its stable ID. */
	Chunk& ChunkSystem::getChunk(ChunkId id)
	{
		if(!chunks.isActive(id)) danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return chunks[id];
	}

	/** @brief Returns a read-only chunk after validating its stable ID. */
	const Chunk& ChunkSystem::getChunk(ChunkId id) const
	{
		if(!chunks.isActive(id)) danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return chunks[id];
	}

	/** @copydoc ChunkSystem::getChunkWorldPosition */
	glm::dvec3 ChunkSystem::getChunkWorldPosition(ChunkId chunk_id) const
	{
		const Chunk& chunk = getChunk(chunk_id);
		const WorldBody& body = world.world_body_data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation * chunk.position;
	}

	/** @brief Reports whether a chunk ID currently refers to an active pool entry. */
	bool ChunkSystem::isChunkActive(ChunkId id) const { return chunks.isActive(id); }
	/** @brief Returns active chunk IDs for iteration; mutations invalidate the reference. */
	const DynamicArray<ChunkId>& ChunkSystem::getUsedChunkIds() const { return chunks.usedIds(); }
	/** @brief Returns the allocated chunk slot count, including reusable slots. */
	size_t ChunkSystem::getChunkSlotCount() const { return chunks.size(); }
	/** @copydoc ChunkSystem::getChunkPointIds */
	const DynamicArray<PointId>& ChunkSystem::getChunkPointIds(ChunkId id) const
	{
		if(!isChunkActive(id) || id >= point_ids_by_chunk.size())
			danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return point_ids_by_chunk[id];
	}
	/** @copydoc ChunkSystem::getBuildPartSlotIds */
	const DynamicArray<PointComponentSlotId>& ChunkSystem::getBuildPartSlotIds(ChunkId id) const
	{
		if(!isChunkActive(id) || id >= build_part_slots_by_chunk.size())
			danikk_engine::formatFatalError("Invalid chunk ID %", id);
		return build_part_slots_by_chunk[id];
	}

	/** @brief Allocates a chunk and adds its reverse membership to the owning body. */
	Chunk& ChunkSystem::createChunk(WorldBodyId world_body_id)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Chunk references invalid world body %", world_body_id);
		const ChunkId id = chunks.create();
		if(id >= point_ids_by_chunk.size()) point_ids_by_chunk.resize(id + 1);
		if(id >= build_part_slots_by_chunk.size()) build_part_slots_by_chunk.resize(id + 1);
		point_ids_by_chunk[id].clear();
		build_part_slots_by_chunk[id].clear();
		ChunkRenderRecord& render_record = getRenderRecord(id);
		render_record.build_part_mesh_dirty = false;
		render_record.render_enabled = true;
		render_record.has_build_part_mesh = false;
		render_record.bounds_valid = false;
		render_record.build_part_mesh_revision = 0;
		render_record.bounds_center = vec3(0.0f);
		render_record.bounds_radius = 0.0f;
		Chunk& chunk = chunks[id];
		chunk.chunk_id = id;
		chunk.world_body_id = world_body_id;
		chunk.grid_coordinate = glm::ivec3(0);
		world.world_body_data.getWorldBody(world_body_id).addChunk(id);
		return chunk;
	}

	/** @brief Registers a point under its owning body and tracks the reverse spatial-cell reference. */
	void ChunkSystem::addPointReferenceToChunk(ChunkId chunk_id, PointId point_id)
	{
		if(!isChunkActive(chunk_id) || !world.point_data.isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach point % to invalid chunk %", point_id, chunk_id);
		Chunk& chunk = getChunk(chunk_id);
		Point& point = world.point_data.getPoint(point_id);
		const WorldBodyId body_id = chunk.world_body_id;
		if(point.world_body_id != static_cast<WorldBodyId>(-1)
			&& point.world_body_id != body_id)
			danikk_engine::formatFatalError("Point % is attached to multiple WorldBodies", point_id);
		WorldBody& body = world.world_body_data.getWorldBody(body_id);
		if(point.world_body_id == static_cast<WorldBodyId>(-1))
		{
			point.world_body_id = body_id;
			point.world_body_point_index = body.point_ids.size();
			body.point_ids.push(point_id);
		}
		else if(point.world_body_point_index >= body.point_ids.size()
			|| body.point_ids[point.world_body_point_index] != point_id)
			danikk_engine::formatFatalError("Point % has an invalid WorldBody index", point_id);
		insertWorldResourceId(point_ids_by_chunk[chunk_id], point_id);
		insertWorldResourceId(point.referencing_chunk_ids, chunk_id);
	}

	/** @copydoc ChunkSystem::removePointReferenceFromChunk */
	bool ChunkSystem::removePointReferenceFromChunk(ChunkId chunk_id, PointId point_id)
	{
		if(!isChunkActive(chunk_id) || !world.point_data.isPointActive(point_id)) return false;
		Chunk& chunk = getChunk(chunk_id);
		Point& point = world.point_data.getPoint(point_id);
		if(!eraseWorldResourceId(point_ids_by_chunk[chunk_id], point_id)) return false;
		if(!eraseWorldResourceId(point.referencing_chunk_ids, chunk_id))
			danikk_engine::formatFatalError("Point % is missing reverse reference to chunk %", point_id, chunk_id);
		if(point.world_body_id != chunk.world_body_id)
			danikk_engine::formatFatalError("Point % is missing reverse reference to chunk %", point_id, chunk_id);
		return true;
	}

	/** @brief Registers a component slot in its chunk and tracks the referenced point. */
	void ChunkSystem::addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id)
	{
		getChunk(chunk_id);
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
		if(slot.chunk_id != PointComponentSlot::no_chunk)
			danikk_engine::formatFatalError("Point component % already belongs to chunk %", slot_id, slot.chunk_id);

		addPointReferenceToChunk(chunk_id, slot.point_id);
		if(slot.type_id == PointComponentTypeId::build_part)
		{
			insertWorldResourceId(build_part_slots_by_chunk[chunk_id], slot_id);
			invalidateBuildPartMesh(chunk_id);
		}
		slot.chunk_id = chunk_id;
	}

	/** @brief Removes a component from chunk scheduling while retaining its payload. */
	void ChunkSystem::removePointComponentSlotFromChunk(PointComponentSlotId slot_id)
	{
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
		if(slot.chunk_id == PointComponentSlot::no_chunk) return;
		getChunk(slot.chunk_id);
		if(slot.type_id == PointComponentTypeId::build_part)
		{
			invalidateBuildPartMesh(slot.chunk_id);
			if(!eraseWorldResourceId(build_part_slots_by_chunk[slot.chunk_id], slot_id))
				danikk_engine::formatFatalError("BuildPart component % is missing from owning spatial cell %", slot_id, slot.chunk_id);
		}
		slot.chunk_id = PointComponentSlot::no_chunk;
	}

	/** @brief Detaches a point from all chunks before PointData destroys its components and slot. */
	void ChunkSystem::destroyPoint(PointId point_id)
	{
		DSE_WATCH();
		if(!world.point_data.isPointActive(point_id)) return;
		Point& point = world.point_data.getPoint(point_id);
		while(point.referencing_chunk_ids.size() > 0)
			removePointReferenceFromChunk(point.referencing_chunk_ids[point.referencing_chunk_ids.size() - 1], point_id);
		while(point.component_slot_ids.size() > 0)
			removePointComponentSlotFromChunk(point.component_slot_ids[point.component_slot_ids.size() - 1]);
		if(point.world_body_id != static_cast<WorldBodyId>(-1)
			&& world.world_body_data.isWorldBodyActive(point.world_body_id)
			&& !removePointFromBody(world,
				world.world_body_data.getWorldBody(point.world_body_id), point_id))
			danikk_engine::formatFatalError("Point % is missing from its owning WorldBody", point_id);
		world.point_data.destroyPoint(point_id);
	}

	/** @brief Destroys chunk components, mesh, point references, and pooled chunk storage. */
	void ChunkSystem::destroyChunk(ChunkId id)
	{
		DSE_WATCH();
		if(!isChunkActive(id)) return;
		Chunk& chunk = getChunk(id);
		destroyRenderData(chunk);
		WorldBody* body = world.world_body_data.isWorldBodyActive(chunk.world_body_id)
			? &world.world_body_data.getWorldBody(chunk.world_body_id) : nullptr;
		if(body != nullptr) body->removeChunk(id);

		while(build_part_slots_by_chunk[id].size() > 0)
		{
			const PointComponentSlotId slot_id = build_part_slots_by_chunk[id][build_part_slots_by_chunk[id].size() - 1];
			removePointComponentSlotFromChunk(slot_id);
			world.point_data.destroyPointComponentSlot(slot_id);
		}
		while(point_ids_by_chunk[id].size() > 0)
		{
			const PointId point_id = point_ids_by_chunk[id][point_ids_by_chunk[id].size() - 1];
			if(!world.point_data.isPointActive(point_id))
			{
				point_ids_by_chunk[id].resize(point_ids_by_chunk[id].size() - 1);
				if(body != nullptr) removePointFromBody(world, *body, point_id);
				continue;
			}
			if(!removePointReferenceFromChunk(id, point_id)) continue;
			Point& point = world.point_data.getPoint(point_id);
			for(size_t slot_index = point.component_slot_ids.size(); slot_index > 0;)
			{
				--slot_index;
				if(slot_index >= point.component_slot_ids.size()) continue;
				const PointComponentSlotId slot_id = point.component_slot_ids[slot_index];
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				if(world.point_data.getPointComponentSlot(slot_id).chunk_id != id) continue;
				removePointComponentSlotFromChunk(slot_id);
				world.point_data.destroyPointComponentSlot(slot_id);
			}
			if(point.referencing_chunk_ids.size() == 0) destroyPoint(point_id);
		}

		chunk.world_body_id = 0;
		chunk.grid_coordinate = glm::ivec3(0);
		chunk.position = glm::dvec3(0.0);
		point_ids_by_chunk[id].clear();
		build_part_slots_by_chunk[id].clear();
		chunks.free(id);
	}

	/** @brief Destroys all chunks and their meshes, retaining pool capacity. */
	void ChunkSystem::clear()
	{
		DSE_WATCH();
		while(chunks.usedIds().size() > 0) destroyChunk(chunks.usedIds()[chunks.usedIds().size() - 1]);
		chunks.clear();
		point_ids_by_chunk.clear();
		build_part_slots_by_chunk.clear();
		clearRenderData();
	}
}
