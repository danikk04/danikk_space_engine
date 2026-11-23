#pragma once

#include <danikk_space_engine/geometry/cube.h>
#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{
	/** @brief Primitive construction shapes supported by BuildPart instances. */
	enum class BuildPartShape : uint8
	{
		cube = 0, ///< Unit cube centered on the local origin.
		slope, ///< Unit triangular prism with one inclined face.
		count ///< Number of supported primitive shapes.
	};

	/** @brief Cached local vertices and triangle indices shared by rendering, raycast, and physics. */
	struct BuildPartGeometry
	{
		/** @brief Local-space vertices shared by all instances of this primitive. */
		DynamicArray<vec3> positions;
		/** @brief Outward-wound triangles indexing positions. */
		DynamicArray<CubeTriangle> triangles;
	};

	/** @brief Returns immutable cached geometry for one supported primitive shape. */
	const BuildPartGeometry& getBuildPartGeometry(BuildPartShape shape);

	/**
	 * @brief Generates local-space vertices and outward-wound triangles for a build-part shape.
	 * @param shape Primitive shape to generate.
	 * @param positions Receives vertices centered at the local origin.
	 * @param triangles Receives triangle indices into positions.
	 */
	void generateBuildPartGeometry(
		BuildPartShape shape,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles);

	/** @brief Creates a rotation matrix that aligns the shape's local Z axis with an outward normal. */
	mat4 createBuildPartRotation(const vec3& up);
}
