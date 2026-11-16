#pragma once

#include <danikk_space_engine/default.h>

namespace danikk_space_engine
{

	/**
	 * @brief Stores a ray origin and its direction in the coordinate space of a query.
	 */
	struct Ray
	{
		/** @brief Ray origin in the coordinate space of the tested triangle. */
		vec3 origin;
		/** @brief Forward direction; intersection normalizes it, so its input magnitude is ignored. */
		vec3 direction;
	};

	/**
	 * @brief Stores whether a triangle was hit and the hit distance and face normal.
	 */
	struct RayTriangleIntersection
	{
		/** @brief True when the forward half-ray intersects the triangle, including its edges. */
		bool hit = false;
		/** @brief Distance from ray origin, measured along the normalized direction. */
		float distance = 0.0f;
		/** @brief Normalized geometric triangle normal; zero when there is no hit. */
		vec3 normal = vec3(0.0f);
	};

	/**
	 * @brief Tests a forward ray against a triangle using the Möller–Trumbore intersection test.
	 * @param ray Ray whose direction may have any non-zero magnitude.
	 * @param a First triangle vertex.
	 * @param b Second triangle vertex.
	 * @param c Third triangle vertex.
	 * @return A hit flag, forward distance, and normalized winding-derived normal;
	 *         a zero-direction ray, degenerate/parallel triangle, outside hit, or
	 *         intersection behind the origin returns the default miss value.
	 * The test is two-sided and includes triangle boundaries; it performs no
	 * back-face culling and has no maximum-distance limit. Distance is measured
	 * in the same units as the vertex coordinates because the ray direction is
	 * normalized before intersection.
	 * @see World::raycast(), which applies chunk broadphase before testing triangles.
	 */
	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c);
}
