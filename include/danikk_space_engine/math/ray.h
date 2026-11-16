#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Represents ray data and behavior.
	 */
	struct Ray
	{
		vec3 origin;
		vec3 direction;
	};

	/**
	 * @brief Represents ray triangle intersection data and behavior.
	 */
	struct RayTriangleIntersection
	{
		bool hit = false;
		float distance = 0.0f;
		vec3 normal = vec3(0.0f);
	};

	/**
	 * @brief Performs the intersect ray triangle operation.
	 */
	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c);
}
