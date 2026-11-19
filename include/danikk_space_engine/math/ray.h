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
	 * @see RaycastSystem::cast(), which applies chunk broadphase before testing triangles.
	 */
	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c);

	/**
	 * @brief Tests a forward ray against a capsule and reports the closest-axis ray distance.
	 * @param origin Ray origin in world coordinates.
	 * @param direction Ray direction; need not be normalized.
	 * @param center Capsule center.
	 * @param up Unit capsule-axis direction.
	 * @param height Total capsule height including hemispherical ends.
	 * @param radius Capsule radius.
	 * @param ray_distance Receives the non-negative distance to the closest point on the capsule axis.
	 * @return True when the closest ray-to-axis distance is no greater than the capsule radius.
	 */
	bool intersectRayCapsule(
		const glm::dvec3& origin, const glm::dvec3& direction,
		const glm::dvec3& center, const glm::dvec3& up,
		double height, double radius, double& ray_distance);
}
