#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	struct CubeTriangle
	{
		uint8 points[3]{};
	};

	void generateCubeGeometry(
		float size,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles);
}
