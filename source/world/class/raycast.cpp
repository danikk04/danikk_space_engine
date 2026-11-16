#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/world/build_part_shape.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Tests whether a finite, normalized ray segment passes through a sphere.
		 * @param center Sphere center relative to the ray origin.
		 * @param radius Sphere radius.
		 * @param direction Normalized ray direction.
		 * @param max_distance Maximum ray-segment length.
		 * @return True if the sphere's chord overlaps the forward segment [0, max_distance].
		 */
		bool rayIntersectsSphere(const vec3& center, float radius, const vec3& direction, float max_distance)
		{
			const float projection = glm::dot(center, direction);
			const float center_distance_squared = glm::dot(center, center);
			const float perpendicular_distance_squared = center_distance_squared - projection * projection;
			const float radius_squared = radius * radius;
			if(perpendicular_distance_squared > radius_squared) return false;

			const float half_chord = glm::sqrt(glm::max(0.0f, radius_squared - perpendicular_distance_squared));
			return projection + half_chord >= 0.0f && projection - half_chord <= max_distance;
		}

		/**
		 * @brief Finds the body's active PlanetComponent, if present.
		 * @param data WorldData used to validate component slot IDs.
		 * @param body Body whose component list is inspected.
		 * @return Planet payload or null when this body has no planet component.
		 */
		const PlanetComponent* getPlanetComponent(const WorldData& data, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id == WorldBodyComponentTypeId::planet)
				{
					return static_cast<const PlanetComponent*>(slot.getData());
				}
			}
			return nullptr;
		}
	}

	/**
	 * @brief Traverses likely chunks, intersects their triangles, and records the nearest hit.
	 *
	 * Planet bodies use a body-level sphere test followed by 3D DDA through the chunk-coordinate
	 * grid. Other bodies scan their chunk IDs. Per-chunk bounds reject misses before ray-triangle
	 * tests; dirty or invalid bounds do not cull the chunk. The DDA visits only grid cells crossed
	 * by the ray, then the shared triangle routine tests geometry in candidate chunks.
	 * @param origin Ray origin in world coordinates.
	 * @param direction Ray direction, normalized internally.
	 * @param max_distance Maximum accepted distance in world units.
	 * @return Nearest triangle hit or a default miss result.
	 */
	RaycastHit World::raycast(const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("World::raycast");
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f) return result;

		const vec3 normalized_direction = direction / direction_length;
		const Ray ray{vec3(0.0f), normalized_direction};
		float nearest_distance = max_distance;

		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			const PlanetComponent* planet = getPlanetComponent(data, body);
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_origin = inverse_rotation * (origin - body.position);
			const vec3 local_direction = vec3(inverse_rotation * glm::dvec3(normalized_direction));
			const Ray body_local_ray{vec3(local_origin), local_direction};

			auto test_chunk = [&](ChunkId chunk_id)
			{
				if(!data.isChunkActive(chunk_id)) return;
				const Chunk& chunk = data.getChunk(chunk_id);
				if(chunk.world_body_id != body_id) return;
				const vec3 chunk_relative_position = vec3(getChunkWorldPosition(chunk_id) - origin);
				const vec3 rotated_bounds_center = vec3(body.rotation * glm::dvec3(chunk.bounds_center));
				if(chunk.hasValidBounds() && !chunk.mesh_dirty
					&& !rayIntersectsSphere(chunk_relative_position + rotated_bounds_center,
						chunk.bounds_radius, normalized_direction, nearest_distance)) return;

				for(GroundTriangleId triangle_id : chunk.ground_triangle_ids)
				{
					if(!data.isGroundTriangleActive(triangle_id)) continue;
					const GroundTriangle& triangle = data.getGroundTriangle(triangle_id);
					const vec3 a = chunk_relative_position
						+ vec3(body.rotation * glm::dvec3(data.getPoint(triangle.points[0]).position));
					const vec3 b = chunk_relative_position
						+ vec3(body.rotation * glm::dvec3(data.getPoint(triangle.points[1]).position));
					const vec3 c = chunk_relative_position
						+ vec3(body.rotation * glm::dvec3(data.getPoint(triangle.points[2]).position));
					const RayTriangleIntersection intersection = intersectRayTriangle(ray, a, b, c);
					if(!intersection.hit || intersection.distance > nearest_distance) continue;

					nearest_distance = intersection.distance;
					result.hit = true;
					result.distance = intersection.distance;
					result.position = origin + glm::dvec3(normalized_direction) * static_cast<double>(intersection.distance);
					result.normal = intersection.normal;
					result.body_id = body_id;
					result.chunk_id = chunk_id;
					result.face = FacePtr{chunk_id, triangle_id};
					result.triangle_index = 0;
					for(uint8 point_index = 0; point_index < 3; point_index++) result.points[point_index] = triangle.points[point_index];
				}

				for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
				{
					if(!data.isPointComponentSlotActive(slot_id)) continue;
					const PointComponentSlot& slot = data.getPointComponentSlot(slot_id);
					if(slot.type_id != PointComponentTypeId::build_part
						|| !data.isPointActive(slot.point_id)) continue;
					const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
					const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
					const glm::dvec3 anchor = chunk.position + glm::dvec3(data.getPoint(slot.point_id).position);
					for(const CubeTriangle& triangle : geometry.triangles)
					{
						const vec3 a = vec3(anchor + glm::dvec3(part.rotation * vec4(geometry.positions[triangle.points[0]], 0.0f)));
						const vec3 b = vec3(anchor + glm::dvec3(part.rotation * vec4(geometry.positions[triangle.points[1]], 0.0f)));
						const vec3 c = vec3(anchor + glm::dvec3(part.rotation * vec4(geometry.positions[triangle.points[2]], 0.0f)));
						const RayTriangleIntersection intersection = intersectRayTriangle(body_local_ray, a, b, c);
						if(!intersection.hit || intersection.distance > nearest_distance) continue;

						nearest_distance = intersection.distance;
						result.hit = true;
						result.hit_build_part = true;
						result.build_part_slot_id = slot_id;
						result.distance = intersection.distance;
						result.position = origin + glm::dvec3(normalized_direction) * static_cast<double>(intersection.distance);
						result.normal = vec3(body.rotation * glm::dvec3(intersection.normal));
						result.body_id = body_id;
						result.chunk_id = chunk_id;
						result.face = FacePtr{};
						result.triangle_index = 0;
						for(uint8 point_index = 0; point_index < 3; point_index++) result.points[point_index] = 0;
					}
				}
			};

			if(planet != nullptr)
			{
				const double broadphase_radius = planet->radius + planet->chunk_load_radius + planet_chunk_width;
				if(!rayIntersectsSphere(
					vec3(body.position - origin),
					static_cast<float>(broadphase_radius),
					normalized_direction,
					nearest_distance)) continue;

				const glm::dvec3 grid_origin = local_origin / planet_chunk_width;
				const glm::dvec3 grid_direction = glm::dvec3(local_direction);
				glm::ivec3 cell;
				glm::ivec3 step;
				glm::dvec3 next_boundary;
				glm::dvec3 boundary_delta;
				for(uint8 axis = 0; axis < 3; axis++)
				{
					const double coordinate = grid_origin[axis];
					const double floored_coordinate = std::floor(coordinate);
					cell[axis] = static_cast<int>(floored_coordinate);
					if(grid_direction[axis] < 0.0 && coordinate == floored_coordinate) cell[axis]--;

					if(grid_direction[axis] > 0.0)
					{
						step[axis] = 1;
						next_boundary[axis] = ((static_cast<double>(cell[axis]) + 1.0) * planet_chunk_width - local_origin[axis])
							/ grid_direction[axis];
						boundary_delta[axis] = planet_chunk_width / grid_direction[axis];
					}
					else if(grid_direction[axis] < 0.0)
					{
						step[axis] = -1;
						next_boundary[axis] = (static_cast<double>(cell[axis]) * planet_chunk_width - local_origin[axis])
							/ grid_direction[axis];
						boundary_delta[axis] = -planet_chunk_width / grid_direction[axis];
					}
					else
					{
						step[axis] = 0;
						next_boundary[axis] = std::numeric_limits<double>::infinity();
						boundary_delta[axis] = std::numeric_limits<double>::infinity();
					}
				}

				double distance_along_ray = 0.0;
				while(distance_along_ray <= nearest_distance)
				{
					ChunkId chunk_id;
					if(planet->chunk_lookup.getChunk(cell, chunk_id)) test_chunk(chunk_id);

					uint8 next_axis = 0;
					if(next_boundary.y < next_boundary.x) next_axis = 1;
					if(next_boundary.z < next_boundary[next_axis]) next_axis = 2;
					distance_along_ray = next_boundary[next_axis];
					if(!std::isfinite(distance_along_ray)) break;
					cell[next_axis] += step[next_axis];
					next_boundary[next_axis] += boundary_delta[next_axis];
				}
			}
			else
			{
				for(ChunkId chunk_id : body.chunk_ids) test_chunk(chunk_id);
			}
		}
		return result;
	}

	/**
	 * @brief Uses the active camera point and forward vector as the ray origin and direction.
	 * @param max_distance Maximum accepted ray length.
	 * @return Nearest hit, or a miss if the camera slot/point is not active.
	 */
	RaycastHit World::raycastFromCamera(float max_distance) const
	{
		if(camera_component_slot_id >= data.getPointComponentSlotCount()) return RaycastHit{};
		if(!data.isPointComponentSlotActive(camera_component_slot_id)) return RaycastHit{};
		const PointComponentSlot& camera_slot = data.getPointComponentSlot(camera_component_slot_id);
		if(!data.isPointActive(camera_slot.point_id)) return RaycastHit{};
		return raycast(getCameraWorldPosition(), getCameraComponent().getFront(), max_distance);
	}
}
