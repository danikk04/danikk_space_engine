#include <danikk_space_engine/shape/collision_matrix.h>
#include <danikk_space_engine/object/gameobject.h>

#include <cmath>
#include <limits>

namespace danikk_space_engine
{

	/**
	 * @brief Stores the center, axes, and half-extents used by oriented-box collision tests.
	 */
	struct OrientedBox
	{
		vec3 center;
		vec3 axes[3];
		vec3 half_extents;
	};

	/**
	 * @brief Extracts a unit oriented box from a GameObject's local transform.
	 * @param object Object whose transformed origin and basis vectors define the box.
	 * @return Box centered at the transformed origin with normalized axes and half
	 *         extents equal to half the basis-vector lengths.
	 */
	static OrientedBox makeUnitBox(const GameObject& object)
	{
		const mat4& transform = object.local_matrix;
		OrientedBox result;
		result.center = vec3(transform * vec4(0.0f, 0.0f, 0.0f, 1.0f));
		for(index_t axis = 0; axis < 3; ++axis)
		{
			const vec3 basis = vec3(transform[axis]);
			const float scale = glm::length(basis);
			result.axes[axis] = scale > 0.000001f ? basis / scale : vec3(0.0f);
			result.half_extents[axis] = scale * 0.5f;
		}
		return result;
	}

	/**
	 * @brief Tests two transformed unit boxes using the 15-axis separating-axis theorem.
	 * @param object_a First box.
	 * @param object_b Second box.
	 * @return Collision flag, minimum-overlap axis oriented from a toward b, and
	 *         penetration depth. Touching projections count as non-colliding.
	 */
	static CollisionInfo checkCubeCubeCollision(const GameObject& object_a, const GameObject& object_b)
	{
		CollisionInfo result;
		const OrientedBox box_a = makeUnitBox(object_a);
		const OrientedBox box_b = makeUnitBox(object_b);
		result.penetration = std::numeric_limits<float>::max();
		const vec3 center_delta = box_b.center - box_a.center;
		auto test_axis = [&](vec3 axis)
		{
			const float axis_length = glm::length(axis);
			if(axis_length <= 0.000001f) return true;
			axis /= axis_length;

			float radius_a = 0.0f;
			float radius_b = 0.0f;
			for(index_t index = 0; index < 3; ++index)
			{
				radius_a += box_a.half_extents[index] * abs(glm::dot(box_a.axes[index], axis));
				radius_b += box_b.half_extents[index] * abs(glm::dot(box_b.axes[index], axis));
			}

			const float signed_distance = glm::dot(center_delta, axis);
			const float penetration = radius_a + radius_b - abs(signed_distance);
			if(penetration <= 0.0f) return false;
			if(penetration < result.penetration)
			{
				result.penetration = penetration;
				result.normal = signed_distance < 0.0f ? -axis : axis;
			}
			return true;
		};

		for(index_t index = 0; index < 3; ++index)
			if(!test_axis(box_a.axes[index]) || !test_axis(box_b.axes[index])) return result;
		for(index_t a = 0; a < 3; ++a)
			for(index_t b = 0; b < 3; ++b)
				if(!test_axis(glm::cross(box_a.axes[a], box_b.axes[b]))) return result;

		result.collided = true;
		return result;
	}

	/**
	 * @brief Returns the sphere center from the object's transformed local origin.
	 * @param object Object whose transform defines the sphere center.
	 * @return World-space center in the same coordinate space as local_matrix.
	 */
	static vec3 getSphereCenter(const GameObject& object)
	{
		return vec3(object.local_matrix * vec4(0.0f, 0.0f, 0.0f, 1.0f));
	}

	/**
	 * @brief Estimates a sphere radius as half the largest transformed basis-vector length.
	 * @param object Object whose transform provides scale.
	 * @return Conservative radius for non-uniformly scaled sphere geometry.
	 */
	static float getSphereRadius(const GameObject& object)
	{
		const mat4& transform = object.local_matrix;
		return 0.5f * glm::max(glm::length(vec3(transform[0])),
				glm::max(glm::length(vec3(transform[1])), glm::length(vec3(transform[2]))));
	}

	/**
	 * @brief Tests two spheres by center distance and summed radii.
	 * @param object_a First sphere object.
	 * @param object_b Second sphere object.
	 * @return Overlap result; coincident centers use the positive X axis as a fallback normal.
	 */
	static CollisionInfo checkSphereSphereCollision(const GameObject& object_a, const GameObject& object_b)
	{
		CollisionInfo result;
		const vec3 delta = getSphereCenter(object_b) - getSphereCenter(object_a);
		const float distance_squared = glm::dot(delta, delta);
		const float radius_sum = getSphereRadius(object_a) + getSphereRadius(object_b);
		if(distance_squared >= radius_sum * radius_sum) return result;

		const float distance = sqrt(distance_squared);
		result.normal = distance > 0.000001f ? delta / distance : vec3(1.0f, 0.0f, 0.0f);
		result.penetration = radius_sum - distance;
		result.collided = true;
		return result;
	}

	/**
	 * @brief Tests a sphere against an oriented unit box using the closest box point.
	 * @param cube_object Box object.
	 * @param sphere_object Sphere object.
	 * @return Overlap normal pointing from the box toward the sphere and penetration.
	 * When the center is inside the box, the nearest box face supplies the normal.
	 */
	static CollisionInfo checkCubeSphereCollision(const GameObject& cube_object, const GameObject& sphere_object)
	{
		CollisionInfo result;
		const OrientedBox box = makeUnitBox(cube_object);
		const vec3 sphere_center = getSphereCenter(sphere_object);
		const float sphere_radius = getSphereRadius(sphere_object);
		const vec3 relative_center = sphere_center - box.center;
		vec3 closest_point = box.center;
		vec3 local_coordinates(0.0f);

		for(index_t axis = 0; axis < 3; ++axis)
		{
			local_coordinates[axis] = glm::dot(relative_center, box.axes[axis]);
			const float clamped = glm::clamp(local_coordinates[axis],
					-box.half_extents[axis], box.half_extents[axis]);
			closest_point += box.axes[axis] * clamped;
		}

		const vec3 delta = sphere_center - closest_point;
		const float distance_squared = glm::dot(delta, delta);
		if(distance_squared >= sphere_radius * sphere_radius) return result;

		const float distance = sqrt(distance_squared);
		if(distance > 0.000001f)
		{
			result.normal = delta / distance;
			result.penetration = sphere_radius - distance;
		}
		else
		{
			index_t nearest_axis = 0;
			float nearest_face_distance = box.half_extents[0] - abs(local_coordinates[0]);
			for(index_t axis = 1; axis < 3; ++axis)
			{
				const float face_distance = box.half_extents[axis] - abs(local_coordinates[axis]);
				if(face_distance < nearest_face_distance)
				{
					nearest_axis = axis;
					nearest_face_distance = face_distance;
				}
			}
			const float side = local_coordinates[nearest_axis] < 0.0f ? -1.0f : 1.0f;
			result.normal = box.axes[nearest_axis] * side;
			result.penetration = sphere_radius + nearest_face_distance;
		}
		result.collided = true;
		return result;
	}

	/**
	 * @brief Tests a sphere-box pair by reversing the box-sphere contact normal.
	 * @param sphere_object First shape in the ordered pair.
	 * @param cube_object Second shape in the ordered pair.
	 * @return Same penetration as the reversed box-sphere test, with normal oriented
	 *         from the sphere toward the box.
	 */
	static CollisionInfo checkSphereCubeCollision(const GameObject& sphere_object, const GameObject& cube_object)
	{
		CollisionInfo result = checkCubeSphereCollision(cube_object, sphere_object);
		result.normal = -result.normal;
		return result;
	}

	/**
	 * @brief Returns a default non-collision result for unsupported or empty shape pairs.
	 * @param unused_a First object; intentionally not inspected.
	 * @param unused_b Second object; intentionally not inspected.
	 * @return Default CollisionInfo with collided false.
	 */
	static CollisionInfo checkNoCollision(const GameObject&, const GameObject&)
	{
		return CollisionInfo();
	}

	const CollisionFunction collision_matrix[ShapeType::count][ShapeType::count]
	{
		{checkNoCollision, checkNoCollision, checkNoCollision},
		{checkNoCollision, checkCubeCubeCollision, checkCubeSphereCollision},
		{checkNoCollision, checkSphereCubeCollision, checkSphereSphereCollision}
	};
}
