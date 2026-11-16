#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a chunk-local mesh or placement position and its chunk reference count.
	 *
	 * A point is stored once in WorldData and can be shared by multiple chunks.
	 * Its reference count measures distinct chunk `point_ids` lists, including
	 * references made by triangles, point components, and graph-component endpoints.
	 * It does not count how many triangles or components inside one chunk use it.
	 */
	struct Point
	{
		/** @brief Position relative to the referencing chunk origin, stored in single precision. */
		vec3 position = vec3(0.0f);
		/**
		 * @brief Number of distinct chunks whose `point_ids` lists reference this point (maximum 100).
		 * WorldData increments this once per chunk and frees the point when the
		 * final referencing chunk is destroyed.
		 */
		uint8 referencing_chunk_count = 0;
	};
}
