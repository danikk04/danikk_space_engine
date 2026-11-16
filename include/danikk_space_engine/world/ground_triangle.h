#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{

	/**
	 * @brief Defines one rendered and raycastable surface triangle by three shared point IDs.
	 *
	 * The point order is significant: it determines the face winding and therefore
	 * the computed normal. A triangle belongs to at most one chunk; WorldData keeps
	 * the reverse chunk ID and list index in sync when attaching or removing it.
	 */
	struct GroundTriangle
	{
		/** @brief Sentinel indicating that this triangle is not attached to a chunk. */
		static constexpr uint32 no_chunk = static_cast<uint32>(-1);
		/** @brief Owning chunk ID, or `no_chunk` while detached. */
		uint32 chunk_id = no_chunk;
		/** @brief Index into the owner's `ground_triangle_ids` array for constant-time swap removal. */
		size_t index_in_chunk = static_cast<size_t>(-1);
		/** @brief Material assigned to this triangle and used when building chunk draw ranges. */
		MaterialId material_id = default_materials::stone;
		/** @brief Three shared point IDs defining the triangle and its winding/normal. */
		PointId points[3]{};
	};
}
