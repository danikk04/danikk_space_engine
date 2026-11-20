#include <optional>

#include <danikk_space_engine/systems/chunk/manager.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
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

	/** @brief Allocates a chunk and adds its reverse membership to the owning body. */
	Chunk& ChunkSystem::createChunk(WorldBodyId world_body_id)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Chunk references invalid world body %", world_body_id);
		const ChunkId id = chunks.create();
		Chunk& chunk = chunks[id];
		chunk.chunk_id = id;
		chunk.world_body_id = world_body_id;
		chunk.grid_coordinate = glm::ivec3(0);
		world.world_body_data.getWorldBody(world_body_id).addChunk(id);
		return chunk;
	}

	/** @brief Adds a distinct point reference and increments its chunk owner count. */
	void ChunkSystem::addPointReferenceToChunk(ChunkId chunk_id, PointId point_id)
	{
		if(!isChunkActive(chunk_id) || !world.point_data.isPointActive(point_id))
			danikk_engine::formatFatalError("Cannot attach point % to invalid chunk %", point_id, chunk_id);
		Chunk& chunk = getChunk(chunk_id);
		const size_t index = findWorldResourceId(chunk.point_ids, point_id);
		if(index < chunk.point_ids.size() && chunk.point_ids[index] == point_id) return;

		Point& point = world.point_data.getPoint(point_id);
		if(point.referencing_chunk_count >= 100)
			danikk_engine::formatFatalError("Point % exceeds the maximum of 100 chunk references", point_id);
		insertWorldResourceId(chunk.point_ids, point_id);
		point.referencing_chunk_count++;
	}

	/** @brief Registers a component slot in its chunk and tracks the referenced point. */
	void ChunkSystem::addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id)
	{
		Chunk& chunk = getChunk(chunk_id);
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
		if(slot.chunk_id != PointComponentSlot::no_chunk)
			danikk_engine::formatFatalError("Point component % already belongs to chunk %", slot_id, slot.chunk_id);

		addPointReferenceToChunk(chunk_id, slot.point_id);
		chunk.addPointComponentSlot(slot_id, slot.type_id);
		if(slot.type_id == PointComponentTypeId::human
			&& !static_cast<HumanComponent*>(slot.getData())->hasDeathTimestamp())
			chunk.addCollisionHumanComponentSlot(slot_id);
		if(slot.type_id == PointComponentTypeId::build_part) chunk.invalidateBuildPartMesh();
		slot.chunk_id = chunk_id;
	}

	/** @brief Removes a component from chunk scheduling while retaining its payload. */
	void ChunkSystem::removePointComponentSlotFromChunk(PointComponentSlotId slot_id)
	{
		PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
		if(slot.chunk_id == PointComponentSlot::no_chunk) return;
		Chunk& chunk = getChunk(slot.chunk_id);
		if(slot.type_id == PointComponentTypeId::build_part) chunk.invalidateBuildPartMesh();
		chunk.removePointComponentSlot(slot_id, slot.type_id);
		slot.chunk_id = PointComponentSlot::no_chunk;
	}

	/** @brief Detaches a point from all chunks before PointData destroys its components and slot. */
	void ChunkSystem::destroyPoint(PointId point_id)
	{
		DSE_WATCH();
		if(!world.point_data.isPointActive(point_id)) return;
		for(ChunkId chunk_id : chunks.usedIds()) eraseWorldResourceId(getChunk(chunk_id).point_ids, point_id);
		world.point_data.getPoint(point_id).referencing_chunk_count = 0;
		for(PointComponentSlotId slot_id : world.point_data.getUsedPointComponentSlotIds())
		{
			if(world.point_data.getPointComponentSlot(slot_id).point_id == point_id)
				removePointComponentSlotFromChunk(slot_id);
		}
		world.point_data.destroyPoint(point_id);
	}

	/** @brief Destroys chunk components, mesh, point references, and pooled chunk storage. */
	void ChunkSystem::destroyChunk(ChunkId id)
	{
		DSE_WATCH();
		if(!isChunkActive(id)) return;
		Chunk& chunk = getChunk(id);
		destroyRenderData(chunk);
		if(world.world_body_data.isWorldBodyActive(chunk.world_body_id))
			world.world_body_data.getWorldBody(chunk.world_body_id).removeChunk(id);

		while(chunk.point_component_slot_ids.size() > 0)
		{
			const PointComponentSlotId slot_id = chunk.point_component_slot_ids[chunk.point_component_slot_ids.size() - 1];
			removePointComponentSlotFromChunk(slot_id);
			world.point_data.destroyPointComponentSlot(slot_id);
		}
		while(chunk.point_ids.size() > 0)
		{
			const PointId point_id = chunk.point_ids[chunk.point_ids.size() - 1];
			chunk.point_ids.resize(chunk.point_ids.size() - 1);
			if(!world.point_data.isPointActive(point_id)) continue;
			Point& point = world.point_data.getPoint(point_id);
			if(point.referencing_chunk_count == 0)
				danikk_engine::formatFatalError("Point % has no chunk references while releasing chunk %", point_id, id);
			point.referencing_chunk_count--;
			if(point.referencing_chunk_count == 0) world.point_data.destroyPoint(point_id);
		}

		chunk.world_body_id = 0;
		chunk.grid_coordinate = glm::ivec3(0);
		chunk.position = glm::dvec3(0.0);
		chunk.planet_terrain = false;
		chunk.build_part_mesh_dirty = false;
		chunk.build_part_mesh_revision = 0;
		chunk.bounds_valid = false;
		chunks.free(id);
	}

	/** @brief Destroys all chunks and their meshes, retaining pool capacity. */
	void ChunkSystem::clear()
	{
		DSE_WATCH();
		while(chunks.usedIds().size() > 0) destroyChunk(chunks.usedIds()[chunks.usedIds().size() - 1]);
		chunks.clear();
		clearRenderData();
	}
}
