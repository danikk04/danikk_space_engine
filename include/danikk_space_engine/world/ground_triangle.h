#pragma once

#include <danikk_space_engine/material.h>
#include <danikk_space_engine/world/point.h>

namespace danikk_space_engine
{
	using GroundTriangleId = uint32;

	struct GroundTriangle
	{
		PointId points[3]{};
		MaterialId material = default_materials::sandstone;
	};
}
