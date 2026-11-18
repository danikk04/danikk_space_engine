#include <danikk_space_engine/world/data.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
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
		if(slot.type_id == PointComponentTypeId::human
			&& !static_cast<HumanComponent*>(slot.getData())->hasDeathTimestamp())
			chunk.addCollisionHumanComponentSlot(slot_id);
		if(slot.type_id == PointComponentTypeId::build_part) chunk.invalidateBuildPartMesh();
		slot.chunk_id = chunk_id;
	}
}
