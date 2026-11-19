#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/planet_terrain_system.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/geometry/build_part.h>

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
		 * @param data WorldBodyData used to validate component slot IDs.
		 * @param body Body whose component list is inspected.
		 * @return Planet payload or null when this body has no planet component.
		 */
		const PlanetComponent* getPlanetComponent(const WorldBodyData& data, const WorldBody& body,
			WorldBodyComponentSlotId& component_slot_id)
		{
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id == WorldBodyComponentTypeId::planet)
				{
					component_slot_id = slot_id;
					return static_cast<const PlanetComponent*>(slot.getData());
				}
			}
			return nullptr;
		}
	}

	/** @copydoc World::raycast */
	RaycastHit World::raycast(const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		const auto& body_ids = world_body_data.getUsedWorldBodyIds();
		return raycastBodies(body_ids.begin(), body_ids.size(), origin, direction, max_distance);
	}

	/** @copydoc World::raycastBody */
	RaycastHit World::raycastBody(WorldBodyId body_id, const glm::dvec3& origin,
		const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		if(!world_body_data.isWorldBodyActive(body_id)) return RaycastHit{};
		return raycastBodies(&body_id, 1, origin, direction, max_distance);
	}

	/**
		 * @brief Traverses candidate chunks and records the nearest scalar-field or BuildPart hit.
	 *
	 * Planet bodies use a sphere test followed by 3D DDA through crossed grid cells. Other
		 * bodies scan their chunk IDs. Planet terrain is sampled from the scalar field; BuildParts
		 * are tested against their component-defined triangles.
	 * @param body_ids IDs of active bodies to query.
	 * @param body_count Number of body IDs in the range.
	 * @param origin Ray origin in world coordinates.
	 * @param direction Ray direction, normalized internally.
	 * @param max_distance Maximum accepted distance in world units.
	 * @return Nearest hit or a default miss result.
	 */
	RaycastHit World::raycastBodies(const WorldBodyId* body_ids, size_t body_count,
		const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f) return result;

		const vec3 normalized_direction = direction / direction_length;
		float nearest_distance = max_distance;

		for(size_t body_index = 0; body_index < body_count; ++body_index)
		{
			const WorldBodyId body_id = body_ids[body_index];
			const WorldBody& body = world_body_data.getWorldBody(body_id);
			WorldBodyComponentSlotId planet_component_slot_id = static_cast<WorldBodyComponentSlotId>(-1);
			const PlanetComponent* planet = getPlanetComponent(world_body_data, body, planet_component_slot_id);
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_origin = inverse_rotation * (origin - body.position);
			const vec3 local_direction = vec3(inverse_rotation * glm::dvec3(normalized_direction));
			const Ray body_local_ray{vec3(local_origin), local_direction};

			auto test_chunk = [&](ChunkId chunk_id)
			{
				if(!chunk_system.isChunkActive(chunk_id)) return;
				const Chunk& chunk = chunk_system.getChunk(chunk_id);
				if(chunk.world_body_id != body_id) return;
				const vec3 chunk_relative_position = vec3(getChunkWorldPosition(chunk_id) - origin);
				const vec3 rotated_bounds_center = vec3(body.rotation * glm::dvec3(chunk.bounds_center));
				if(chunk.hasValidBounds()
					&& !rayIntersectsSphere(chunk_relative_position + rotated_bounds_center,
						chunk.bounds_radius, normalized_direction, nearest_distance)) return;

				for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
				{
					if(!point_data.isPointComponentSlotActive(slot_id)) continue;
					const PointComponentSlot& slot = point_data.getPointComponentSlot(slot_id);
					if(slot.type_id != PointComponentTypeId::build_part
						|| !point_data.isPointActive(slot.point_id)) continue;
					const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
					const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
					const glm::dvec3 anchor = chunk.position + glm::dvec3(point_data.getPoint(slot.point_id).position);
					for(const CubeTriangle& triangle : geometry.triangles)
					{
					const vec3 a = vec3(anchor + glm::dvec3(part.rotation
						* vec4(geometry.positions[triangle.points[0]] * part.size, 0.0f)));
					const vec3 b = vec3(anchor + glm::dvec3(part.rotation
						* vec4(geometry.positions[triangle.points[1]] * part.size, 0.0f)));
					const vec3 c = vec3(anchor + glm::dvec3(part.rotation
						* vec4(geometry.positions[triangle.points[2]] * part.size, 0.0f)));
						const RayTriangleIntersection intersection = intersectRayTriangle(body_local_ray, a, b, c);
						if(!intersection.hit || intersection.distance > nearest_distance) continue;

						nearest_distance = intersection.distance;
						result.hit = true;
						result.hit_build_part = true;
						result.hit_terrain = false;
						result.build_part_slot_id = slot_id;
						result.distance = intersection.distance;
						result.position = origin + glm::dvec3(normalized_direction) * static_cast<double>(intersection.distance);
						result.normal = vec3(body.rotation * glm::dvec3(intersection.normal));
						result.body_id = body_id;
						result.chunk_id = chunk_id;
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
					uint8 next_axis = 0;
					if(next_boundary.y < next_boundary.x) next_axis = 1;
					if(next_boundary.z < next_boundary[next_axis]) next_axis = 2;
					const double segment_end = std::min(static_cast<double>(nearest_distance), next_boundary[next_axis]);
					if(planet->chunk_lookup.getChunk(cell, chunk_id) && chunk_system.isChunkActive(chunk_id))
					{
						double field_distance;
						glm::dvec3 field_normal;
		if(segment_end >= distance_along_ray && raycastPlanetScalarField(
			*this, planet_component_slot_id,
							local_origin + glm::dvec3(local_direction) * distance_along_ray,
							glm::dvec3(local_direction), segment_end - distance_along_ray,
							field_distance, field_normal))
						{
							const double hit_distance = distance_along_ray + field_distance;
							nearest_distance = static_cast<float>(hit_distance);
							result = RaycastHit{};
							result.hit = true;
							result.hit_terrain = true;
							result.distance = nearest_distance;
							result.position = origin + glm::dvec3(normalized_direction) * hit_distance;
							result.normal = vec3(body.rotation * field_normal);
							result.body_id = body_id;
							result.chunk_id = chunk_id;
						}
						test_chunk(chunk_id);
					}
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
		if(camera_component_slot_id >= point_data.getPointComponentSlotCount()) return RaycastHit{};
		if(!point_data.isPointComponentSlotActive(camera_component_slot_id)) return RaycastHit{};
		const PointComponentSlot& camera_slot = point_data.getPointComponentSlot(camera_component_slot_id);
		if(!point_data.isPointActive(camera_slot.point_id)) return RaycastHit{};
		return raycast(getCameraWorldPosition(), getCameraComponent().getFront(), max_distance);
	}
}
