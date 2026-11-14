#pragma once

#include <danikk_space_engine/world/volume_cell.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/ground_triangle.h>

namespace danikk_space_engine
{
	struct FacePtr
	{
		ChunkId chunk_id = 0;
		VolumeCellId cell_id = 0;
		uint8 face_index = 0;
		GroundTriangleId ground_triangle_id = 0;
		bool is_ground_triangle = false;

		bool operator==(const FacePtr& other) const
		{
			return chunk_id == other.chunk_id
				&& cell_id == other.cell_id
				&& face_index == other.face_index
				&& ground_triangle_id == other.ground_triangle_id
				&& is_ground_triangle == other.is_ground_triangle;
		}
	};
}
