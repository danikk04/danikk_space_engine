#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the origin and direction used for geometric intersection queries.
	 */
	struct Ray
	{
		vec3 origin;
		vec3 direction;
	};

	/**
	 * @brief Stores a ray hit’s distance and barycentric coordinates on a triangle.
	 */
	struct RayTriangleIntersection
	{
		bool hit = false;
		float distance = 0.0f;
		vec3 normal = vec3(0.0f);
	};

	/**
	 * @brief Tests a ray against a triangle and returns distance and barycentric coordinates on a hit.
	 */
	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c);
}
