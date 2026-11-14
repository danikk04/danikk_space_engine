#pragma once

#include <danikk_space_engine/world/point.h>

namespace danikk_space_engine
{
	using GroundTriangleId = uint32;

	struct GroundTriangle
	{
		PointId points[3]{};
	};
}
