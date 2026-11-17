#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Performs a linear membership check in a compact list of resource IDs.
		 * @tparam id_t ID type stored by the list.
		 * @param ids List to inspect.
		 * @param id Resource ID to find.
		 * @return True when the ID is already present.
		 */
		template<typename id_t>
		bool containsId(const DynamicArray<id_t>& ids, id_t id)
		{
			for(id_t existing_id : ids)
			{
				if(existing_id == id) return true;
			}
			return false;
		}

		/**
		 * @brief Validates and registers one unique resource ID in a chunk-owned list.
		 *
		 * Runs the supplied attachment callback before insertion so reverse ownership links are
		 * established first. Optionally marks the chunk mesh dirty after adding render geometry.
		 * @tparam id_t Resource ID type.
		 * @tparam array_t Chunk member array type.
		 * @tparam attach_t Callback that establishes reverse links or reference counts.
		 * @param world_data Pools used to validate the chunk and resource.
		 * @param chunk_id Chunk receiving the resource.
		 * @param resource_id ID to append to the chunk list.
		 * @param resource_is_active Whether the resource currently exists in its pool.
		 * @param resource_name Resource label used in fatal diagnostics.
		 * @param resource_ids Member pointer selecting the chunk list to update.
		 * @param attach_resource Callback run before the ID is inserted.
		 * @param invalidate_mesh Whether insertion changes rendered geometry.
		 */
		template<typename id_t, typename array_t, typename attach_t>
		void addUniqueResourceToChunk(
			WorldData& world_data,
			ChunkId chunk_id,
			id_t resource_id,
			bool resource_is_active,
			const char* resource_name,
			array_t Chunk::* resource_ids,
			attach_t attach_resource,
			bool invalidate_mesh = false)
		{
			if(!world_data.isChunkActive(chunk_id) || !resource_is_active)
			{
				danikk_engine::formatFatalError(
					"Cannot attach % % to invalid chunk %", resource_name, resource_id, chunk_id);
			}
			Chunk& chunk = world_data.getChunk(chunk_id);
			if(containsId(chunk.*resource_ids, resource_id))
			{
				danikk_engine::formatFatalError(
					"% % is already attached to chunk %", resource_name, resource_id, chunk_id);
			}
			attach_resource();
			(chunk.*resource_ids).push(resource_id);
			if(invalidate_mesh) chunk.invalidateMesh();
		}
	}

	/**
	 * @brief Adds a distinct point reference to a chunk and updates the point's owner count.
	 *
	 * The chunk's sorted point-ID list is checked first, making duplicate attachment a no-op.
	 * The count is incremented only when this chunk becomes a new owner/reference holder and is
	 * capped at 100 to catch unexpected sharing fan-out.
	 * @param chunk_id Active chunk that will reference the point.
	 * @param point_id Active point to reference.
	 */
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
		if(point.referencing_chunk_count >= 100)
		{
			danikk_engine::formatFatalError("Point % exceeds the maximum of 100 chunk references", point_id);
		}
		insertWorldResourceId(chunk.point_ids, point_id);
		point.referencing_chunk_count++;
	}

	/**
	 * @brief Attaches a triangle to exactly one chunk and establishes point references.
	 *
	 * The triangle records its owning chunk and index in the chunk list. Its three point IDs are
	 * registered with the chunk (duplicates among the triangle's vertices do not inflate ownership
	 * counts), and the chunk mesh is invalidated for rebuilding.
	 * @param chunk_id Active chunk that owns the triangle.
	 * @param triangle_id Active triangle not already attached to another chunk.
	 */
	void WorldData::addGroundTriangleToChunk(ChunkId chunk_id, GroundTriangleId triangle_id)
	{
		GroundTriangle& triangle = getGroundTriangle(triangle_id);
		if(triangle.chunk_id != GroundTriangle::no_chunk)
		{
			danikk_engine::formatFatalError("Ground triangle % already belongs to chunk %", triangle_id, triangle.chunk_id);
		}

		addUniqueResourceToChunk(*this, chunk_id, triangle_id, isGroundTriangleActive(triangle_id), "ground triangle",
			&Chunk::ground_triangle_ids,
			[this, chunk_id, triangle_id]
			{
				const GroundTriangle& triangle = getGroundTriangle(triangle_id);
				for(PointId point_id : triangle.points) addPointReferenceToChunk(chunk_id, point_id);
			},
			true);
		Chunk& chunk = getChunk(chunk_id);
		triangle.chunk_id = chunk_id;
		triangle.index_in_chunk = chunk.ground_triangle_ids.size() - 1;
	}

	/**
	 * @brief Attaches a point component to its scheduling chunk and references its point.
	 *
	 * The owning chunk indexes the slot for updates and also caches human slots for spatial queries.
	 * @param chunk_id Active chunk that will tick and frame the component.
	 * @param slot_id Active component whose point belongs to that chunk.
	 */
	void WorldData::addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id)
	{
		Chunk& chunk = getChunk(chunk_id);
		PointComponentSlot& slot = getPointComponentSlot(slot_id);
		if(slot.chunk_id != PointComponentSlot::no_chunk)
			danikk_engine::formatFatalError("Point component % already belongs to chunk %", slot_id, slot.chunk_id);

		addPointReferenceToChunk(chunk_id, slot.point_id);
		chunk.addPointComponentSlot(slot_id, slot.type_id);
		slot.chunk_id = chunk_id;
	}
}
