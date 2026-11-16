#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents cube triangle data and behavior.
	 */
	struct CubeTriangle
	{
		uint8 points[3]{};
	};

	/**
	 * @brief Generates cube geometry.
	 */
	void generateCubeGeometry(
		float size,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles);
}
