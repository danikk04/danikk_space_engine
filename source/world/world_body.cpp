#include <danikk_space_engine/world/world_body.h>

namespace danikk_space_engine
{
	void WorldBody::addChunk(ChunkId chunk_id)
	{
		for(ChunkId existing_chunk_id : chunk_ids)
		{
			if(existing_chunk_id == chunk_id) return;
		}
		chunk_ids.push(chunk_id);
	}

	bool WorldBody::removeChunk(ChunkId chunk_id)
	{
		for(size_t index = 0; index < chunk_ids.size(); index++)
		{
			if(chunk_ids[index] != chunk_id) continue;
			for(size_t shift_index = index; shift_index + 1 < chunk_ids.size(); shift_index++)
			{
				chunk_ids[shift_index] = chunk_ids[shift_index + 1];
			}
			chunk_ids.resize(chunk_ids.size() - 1);
			return true;
		}
		return false;
	}
}
