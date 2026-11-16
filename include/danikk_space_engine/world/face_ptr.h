#pragma once

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/ground_triangle.h>

namespace danikk_space_engine
{

	/**
	 * @brief Identifies a terrain face by its owning chunk and face index.
	 */
	struct FacePtr
	{
		ChunkId chunk_id = 0;
		GroundTriangleId ground_triangle_id = 0;

		bool operator==(const FacePtr& other) const
		{
			return chunk_id == other.chunk_id
				&& ground_triangle_id == other.ground_triangle_id;
		}
	};
}
