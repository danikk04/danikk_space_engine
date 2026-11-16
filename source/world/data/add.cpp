#include <danikk_space_engine/world/data.h>

namespace danikk_space_engine
{
	namespace
	{

		template<typename id_t>
		/**
		 * @brief Performs a linear membership check in the compact ID list.
		 */
		bool containsId(const DynamicArray<id_t>& ids, id_t id)
		{
			for(id_t existing_id : ids)
			{
				if(existing_id == id) return true;
			}
			return false;
		}

		template<typename id_t, typename array_t, typename attach_t>
		/**
		 * @brief Adds unique resource to chunk.
		 */
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
	 * @brief Adds point reference to chunk.
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
	 * @brief Adds ground triangle to chunk.
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
	 * @brief Adds point component slot to chunk.
	 */
	void WorldData::addPointComponentSlotToChunk(ChunkId chunk_id, PointComponentSlotId slot_id)
	{

		addUniqueResourceToChunk(*this, chunk_id, slot_id, isPointComponentSlotActive(slot_id), "point component",
			&Chunk::point_component_slot_ids,
			[this, chunk_id, slot_id]
			{
				addPointReferenceToChunk(chunk_id, getPointComponentSlot(slot_id).point_id);
			});
		getPointComponentSlot(slot_id).chunk_id = chunk_id;
	}
}
