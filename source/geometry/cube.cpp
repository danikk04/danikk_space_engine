#include <danikk_space_engine/geometry/cube.h>

namespace danikk_space_engine
{
	/**
	 * @brief Replaces the output arrays with a centered, shared-vertex cube mesh topology.
	 * @param size Full cube edge length.
	 * @param positions Receives eight corners at plus/minus half-size on each axis.
	 * @param triangles Receives twelve outward-wound triangles, two for each side.
	 */
	void generateCubeGeometry(
		float size,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles)
	{
		const float half_size = size * 0.5f;
		positions.clear();
		triangles.clear();
		positions.reserve(8);
		triangles.reserve(12);
		positions.push(vec3(-half_size, -half_size, -half_size));
		positions.push(vec3( half_size, -half_size, -half_size));
		positions.push(vec3( half_size,  half_size, -half_size));
		positions.push(vec3(-half_size,  half_size, -half_size));
		positions.push(vec3(-half_size, -half_size,  half_size));
		positions.push(vec3( half_size, -half_size,  half_size));
		positions.push(vec3( half_size,  half_size,  half_size));
		positions.push(vec3(-half_size,  half_size,  half_size));

		triangles.push(CubeTriangle{{0, 2, 1}});
		triangles.push(CubeTriangle{{0, 3, 2}});
		triangles.push(CubeTriangle{{4, 5, 6}});
		triangles.push(CubeTriangle{{4, 6, 7}});
		triangles.push(CubeTriangle{{0, 4, 7}});
		triangles.push(CubeTriangle{{0, 7, 3}});
		triangles.push(CubeTriangle{{1, 2, 6}});
		triangles.push(CubeTriangle{{1, 6, 5}});
		triangles.push(CubeTriangle{{0, 1, 5}});
		triangles.push(CubeTriangle{{0, 5, 4}});
		triangles.push(CubeTriangle{{3, 7, 6}});
		triangles.push(CubeTriangle{{3, 6, 2}});
	}
}
