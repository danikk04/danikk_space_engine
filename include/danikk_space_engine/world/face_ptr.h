#pragma once

#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/ground_triangle.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stable handle to a ground triangle, pairing its owning chunk and global pool ID.
	 *
	 * The chunk ID provides spatial ownership; the triangle ID resolves the triangle in WorldData.
	 * This is a value handle, not a pointer or generation-checked reference. Both IDs must still
	 * be active and the triangle must still belong to that chunk when the handle is used, since
	 * freed pool IDs may be reused.
	 */
	struct FacePtr
	{
		/** @brief Chunk that owns the referenced ground triangle. */
		ChunkId chunk_id = 0;
		/** @brief Ground-triangle pool ID of the referenced face. */
		GroundTriangleId ground_triangle_id = 0;

		/**
		 * @brief Compares both resource IDs for exact handle equality.
		 * @param other Handle to compare against.
		 * @return True only when chunk and triangle IDs both match.
		 */
		bool operator==(const FacePtr& other) const
		{
			return chunk_id == other.chunk_id
				&& ground_triangle_id == other.ground_triangle_id;
		}
	};
}
