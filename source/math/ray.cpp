#include <danikk_space_engine/math/ray.h>

#include <cmath>

namespace danikk_space_engine
{
	/**
	 * @brief Tests a ray against one triangle with the Möller–Trumbore algorithm.
	 * @param ray Input ray; its direction is normalized locally.
	 * @param a First triangle vertex.
	 * @param b Second triangle vertex.
	 * @param c Third triangle vertex.
	 * @return Hit flag, distance along the normalized ray, and winding-derived
	 *         normalized face normal. Degenerate, parallel, outside, and
	 *         behind-origin intersections return a default miss.
	 * The test is two-sided: the determinant sign is not used for back-face culling.
	 * @see World::raycast(), which calls this test only for triangles in candidate chunks.
	 */
	RayTriangleIntersection intersectRayTriangle(
		const Ray& ray,
		const vec3& a,
		const vec3& b,
		const vec3& c)
	{
		RayTriangleIntersection result;
		constexpr float epsilon = 0.000001f;
		const float direction_length = glm::length(ray.direction);
		if(direction_length <= epsilon)
		{
			return result;
		}

		const vec3 direction = ray.direction / direction_length;
		const vec3 edge_a = b - a;
		const vec3 edge_b = c - a;
		const vec3 determinant_vector = glm::cross(direction, edge_b);
		const float determinant = glm::dot(edge_a, determinant_vector);

		if(std::fabs(determinant) <= epsilon)
		{
			return result;
		}

		const float inverse_determinant = 1.0f / determinant;
		const vec3 origin_offset = ray.origin - a;
		const float barycentric_u = glm::dot(origin_offset, determinant_vector) * inverse_determinant;
		if(barycentric_u < 0.0f || barycentric_u > 1.0f)
		{
			return result;
		}

		const vec3 barycentric_vector = glm::cross(origin_offset, edge_a);
		const float barycentric_v = glm::dot(direction, barycentric_vector) * inverse_determinant;
		if(barycentric_v < 0.0f || barycentric_u + barycentric_v > 1.0f)
		{
			return result;
		}

		const float distance = glm::dot(edge_b, barycentric_vector) * inverse_determinant;
		if(distance < 0.0f)
		{
			return result;
		}

		result.hit = true;
		result.distance = distance;
		result.normal = glm::normalize(glm::cross(edge_a, edge_b));
		return result;
	}
}
