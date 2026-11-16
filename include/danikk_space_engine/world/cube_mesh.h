#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores three indices into the generated cube's shared corner array.
	 *
	 * Triangle vertex order defines its outward-facing winding.
	 */
	struct CubeTriangle
	{
		uint8 points[3]{};
	};

	/**
	 * @brief Generates a centered cube from eight shared vertices and twelve outward-wound triangles.
	 * @param size Full edge length; the cube extends by `size / 2` on either side of the origin.
	 * @param positions Output array, cleared and replaced with the eight corner positions.
	 * @param triangles Output array, cleared and replaced with two triangles per cube face.
	 */
	void generateCubeGeometry(
		float size,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles);
}
