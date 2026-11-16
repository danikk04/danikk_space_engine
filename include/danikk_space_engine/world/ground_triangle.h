#pragma once

#include <danikk_space_engine/world/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>

namespace danikk_space_engine
{

	/**
	 * @brief Defines one terrain surface triangle by the IDs of its three points.
	 */
	struct GroundTriangle
	{
		static constexpr uint32 no_chunk = static_cast<uint32>(-1);
		uint32 chunk_id = no_chunk;
		size_t index_in_chunk = static_cast<size_t>(-1);
		MaterialId material_id = default_materials::stone;
		PointId points[3]{};
	};
}
