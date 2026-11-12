#pragma once

#include <danikk_space_engine/volume_cell.h>
#include <danikk_space_engine/chunk.h>

namespace danikk_space_engine
{
	struct FacePtr
	{
		ChunkId chunk_id = 0;
		VolumeCellId cell_id = 0;
		uint8 face_index = 0;

		bool operator==(const FacePtr& other) const
		{
			return chunk_id == other.chunk_id
				&& cell_id == other.cell_id
				&& face_index == other.face_index;
		}
	};
}
