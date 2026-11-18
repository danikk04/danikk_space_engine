#include <danikk_space_engine/geometry/build_part.h>

namespace danikk_space_engine
{
	namespace
	{
		BuildPartGeometry build_part_geometries[static_cast<size_t>(BuildPartShape::count)];
		bool build_part_geometry_initialized[static_cast<size_t>(BuildPartShape::count)]{};

		/**
		 * @brief Appends a triangle and flips its winding when its normal points toward the origin.
		 * @param triangles Output triangle list.
		 * @param positions Shape vertex positions used to determine the outward direction.
		 * @param a First vertex index.
		 * @param b Second vertex index.
		 * @param c Third vertex index.
		 */
		void addOutwardTriangle(
			DynamicArray<CubeTriangle>& triangles,
			const DynamicArray<vec3>& positions,
			uint8 a,
			uint8 b,
			uint8 c)
		{
			const vec3 center = (positions[a] + positions[b] + positions[c]) / 3.0f;
			const vec3 normal = glm::cross(positions[b] - positions[a], positions[c] - positions[a]);
			triangles.push(glm::dot(normal, center) >= 0.0f
				? CubeTriangle{{a, b, c}}
				: CubeTriangle{{a, c, b}});
		}
	}

	/** @copydoc getBuildPartGeometry */
	const BuildPartGeometry& getBuildPartGeometry(BuildPartShape shape)
	{
		const size_t index = static_cast<size_t>(shape);
		if(index >= static_cast<size_t>(BuildPartShape::count))
		{
			danikk_engine::formatFatalError("Invalid BuildPart shape %", index);
		}
		if(build_part_geometry_initialized[index]) return build_part_geometries[index];
		BuildPartGeometry& geometry = build_part_geometries[index];
		if(shape == BuildPartShape::cube)
		{
			generateCubeGeometry(1.0f, geometry.positions, geometry.triangles);
		}
		else
		{
			geometry.positions.reserve(6);
			geometry.triangles.reserve(8);
			geometry.positions.push(vec3(-0.5f, -0.5f, -0.5f));
			geometry.positions.push(vec3( 0.5f, -0.5f, -0.5f));
			geometry.positions.push(vec3( 0.5f,  0.5f, -0.5f));
			geometry.positions.push(vec3(-0.5f,  0.5f, -0.5f));
			geometry.positions.push(vec3(-0.5f, -0.5f,  0.5f));
			geometry.positions.push(vec3(-0.5f,  0.5f,  0.5f));

			addOutwardTriangle(geometry.triangles, geometry.positions, 0, 3, 2);
			addOutwardTriangle(geometry.triangles, geometry.positions, 0, 2, 1);
			addOutwardTriangle(geometry.triangles, geometry.positions, 0, 4, 5);
			addOutwardTriangle(geometry.triangles, geometry.positions, 0, 5, 3);
			addOutwardTriangle(geometry.triangles, geometry.positions, 1, 2, 5);
			addOutwardTriangle(geometry.triangles, geometry.positions, 1, 5, 4);
			addOutwardTriangle(geometry.triangles, geometry.positions, 0, 1, 4);
			addOutwardTriangle(geometry.triangles, geometry.positions, 3, 5, 2);
		}
		build_part_geometry_initialized[index] = true;
		return geometry;
	}

	/** @copydoc generateBuildPartGeometry */
	void generateBuildPartGeometry(
		BuildPartShape shape,
		DynamicArray<vec3>& positions,
		DynamicArray<CubeTriangle>& triangles)
	{
		positions.clear();
		triangles.clear();
		const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
		for(const vec3& position : geometry.positions) positions.push(position);
		for(const CubeTriangle& triangle : geometry.triangles) triangles.push(triangle);
	}

	/** @copydoc createBuildPartRotation */
	mat4 createBuildPartRotation(const vec3& requested_up)
	{
		vec3 up = requested_up;
		const float up_length = glm::length(up);
		if(up_length <= 0.0001f) up = vec3(0.0f, 0.0f, 1.0f);
		else up /= up_length;
		const vec3 reference = glm::abs(glm::dot(up, vec3(0.0f, 1.0f, 0.0f))) > 0.99f
			? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
		const vec3 right = glm::normalize(glm::cross(reference, up));
		const vec3 forward = glm::normalize(glm::cross(up, right));
		mat4 rotation(1.0f);
		rotation[0] = vec4(right, 0.0f);
		rotation[1] = vec4(forward, 0.0f);
		rotation[2] = vec4(up, 0.0f);
		return rotation;
	}
}
