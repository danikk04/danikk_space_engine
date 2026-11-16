#include <danikk_space_engine/world/cube_mesh.h>

namespace danikk_space_engine
{
	// Generates a centered cube with shared corner vertices and outward-wound triangle faces.

	/**
	 * @brief Generates cube geometry.
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
