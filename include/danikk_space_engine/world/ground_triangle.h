#pragma once

#include <danikk_space_engine/world/point.h>

namespace danikk_space_engine
{
	using GroundTriangleId = uint32;

	struct GroundTriangle
	{
		static constexpr uint32 no_chunk = static_cast<uint32>(-1);
		uint32 chunk_id = no_chunk;
		size_t index_in_chunk = static_cast<size_t>(-1);
		PointId points[3]{};
	};
}
