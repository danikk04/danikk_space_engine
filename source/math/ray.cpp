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
	 * @see RaycastSystem::cast(), which calls this test only for triangles in octree point candidates.
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

	/** @copydoc intersectRayCapsule */
	bool intersectRayCapsule(
		const glm::dvec3& origin, const glm::dvec3& input_direction,
		const glm::dvec3& center, const glm::dvec3& up,
		double height, double radius, double& ray_distance)
	{
		const double direction_length = glm::length(input_direction);
		if(direction_length <= 1e-12 || height <= 0.0 || radius <= 0.0) return false;
		const glm::dvec3 direction = input_direction / direction_length;
		const glm::dvec3 axis_start = center - up * (height * 0.5 - radius);
		const glm::dvec3 axis = up * (height - radius * 2.0);
		const glm::dvec3 relative = origin - axis_start;
		const double axis_length_squared = glm::dot(axis, axis);
		const double axis_projection = glm::dot(direction, axis);
		const double ray_offset = glm::dot(direction, relative);
		double axis_parameter = 0.0;
		if(axis_length_squared > 1e-12)
		{
			const double denominator = axis_length_squared - axis_projection * axis_projection;
			if(denominator > 1e-12)
			{
				axis_parameter = glm::clamp(
					(glm::dot(axis, relative) - axis_projection * ray_offset) / denominator,
					0.0, 1.0);
			}
		}

		glm::dvec3 closest_axis_point = axis_start + axis * axis_parameter;
		ray_distance = glm::max(glm::dot(closest_axis_point - origin, direction), 0.0);
		const glm::dvec3 ray_point = origin + direction * ray_distance;
		if(axis_length_squared > 1e-12)
		{
			axis_parameter = glm::clamp(
				glm::dot(ray_point - axis_start, axis) / axis_length_squared, 0.0, 1.0);
			closest_axis_point = axis_start + axis * axis_parameter;
			ray_distance = glm::max(glm::dot(closest_axis_point - origin, direction), 0.0);
		}
		const glm::dvec3 separation = origin + direction * ray_distance - closest_axis_point;
		return glm::dot(separation, separation) <= radius * radius;
	}
}
