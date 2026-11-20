#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/geometry/build_part.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void RaycastSystem::init() { danikk_framework::formatLogInfo("Init RaycastSystem"); }
	void RaycastSystem::shutdown() { danikk_framework::formatLogInfo("Shutdown RaycastSystem"); }
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

	}

	RaycastHit RaycastSystem::cast(const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		const auto& body_ids = world.world_body_data.getUsedWorldBodyIds();
		return castBodies(body_ids.begin(), body_ids.size(), origin, direction, max_distance);
	}

	RaycastHit RaycastSystem::castBody(WorldBodyId body_id, const glm::dvec3& origin,
		const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(body_id)) return RaycastHit{};
		return castBodies(&body_id, 1, origin, direction, max_distance);
	}

	/** @brief Finds a sign change in the planet density field and estimates its surface normal. */
	bool RaycastSystem::raycastPlanetScalarField(WorldBodyId planet_body_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		PlanetTerrainMode mode, double& distance, glm::dvec3& normal) const
	{
		DSE_WATCH();
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12 || max_distance < 0.0) return false;
		const glm::dvec3 ray_direction = direction / direction_length;
		const auto densityAt = [&](double offset)
		{
			return world.planets.getPlanetDensity(planet_body_id,
				origin + ray_direction * offset, mode);
		};

		constexpr double step = 0.5;
		double previous_distance = 0.0;
		double previous_density = densityAt(0.0);
		while(previous_distance < max_distance)
		{
			const double next_distance = std::min(previous_distance + step, max_distance);
			const double next_density = densityAt(next_distance);
			if((previous_density <= 0.0 && next_density > 0.0)
				|| (previous_density > 0.0 && next_density <= 0.0))
			{
				double lower = previous_distance;
				double upper = next_distance;
				for(uint8 iteration = 0; iteration < 16; ++iteration)
				{
					const double midpoint = (lower + upper) * 0.5;
					const double midpoint_density = densityAt(midpoint);
					if((midpoint_density > 0.0) == (previous_density > 0.0)) lower = midpoint;
					else upper = midpoint;
				}
				const double hit_distance = (lower + upper) * 0.5;
				const glm::dvec3 hit_position = origin + ray_direction * hit_distance;
				constexpr double gradient_step = 0.05;
				glm::dvec3 gradient(0.0);
				for(uint8 axis = 0; axis < 3; ++axis)
				{
					glm::dvec3 offset(0.0);
					offset[axis] = gradient_step;
					gradient[axis] = world.planets.getPlanetDensity(
						planet_body_id, hit_position + offset, mode)
						- world.planets.getPlanetDensity(planet_body_id, hit_position - offset, mode);
				}
				const double gradient_length = glm::length(gradient);
				if(gradient_length <= 1e-12) return false;
				distance = hit_distance;
				normal = -gradient / gradient_length;
				return true;
			}
			previous_distance = next_distance;
			previous_density = next_density;
		}
		return false;
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
	RaycastHit RaycastSystem::castBodies(const WorldBodyId* body_ids, size_t body_count,
		const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		const World& owner = world;
		const WorldBodyData& world_body_data = owner.world_body_data;
		const PlanetSystem& planets = owner.planets;
		const ChunkSystem& chunks = owner.chunks;
		const PointData& point_data = owner.point_data;
		const auto getChunkWorldPosition = [&chunks](ChunkId chunk_id)
		{
			return chunks.getChunkWorldPosition(chunk_id);
		};
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f) return result;

		const vec3 normalized_direction = direction / direction_length;
		float nearest_distance = max_distance;

		for(size_t body_index = 0; body_index < body_count; ++body_index)
		{
			const WorldBodyId body_id = body_ids[body_index];
			const WorldBody& body = world_body_data.getWorldBody(body_id);
			const bool is_planet = planets.isPlanet(body_id);
			const PlanetData* planet = is_planet ? planets.findPlanet(body_id) : nullptr;
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_origin = inverse_rotation * (origin - body.position);
			const vec3 local_direction = vec3(inverse_rotation * glm::dvec3(normalized_direction));
			const Ray body_local_ray{vec3(local_origin), local_direction};

			auto test_chunk = [&](ChunkId chunk_id)
			{
				if(!chunks.isChunkActive(chunk_id)) return;
				const Chunk& chunk = chunks.getChunk(chunk_id);
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

			if(is_planet)
			{
				const PlanetSettings& planet_settings = planets.getPlanetSettings(planet->settings_id);
				const double broadphase_radius = planet_settings.radius
					+ planet_settings.chunk_load_radius + planet_chunk_width;
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
					if(planet->chunk_lookup.getChunk(cell, chunk_id) && chunks.isChunkActive(chunk_id))
					{
						double field_distance;
						glm::dvec3 field_normal;
						if(segment_end >= distance_along_ray && raycastPlanetScalarField(
							body_id,
							local_origin + glm::dvec3(local_direction) * distance_along_ray,
							glm::dvec3(local_direction), segment_end - distance_along_ray,
							PlanetTerrainMode::cave_field_with_edits,
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
	RaycastHit RaycastSystem::castFromCamera(float max_distance) const
	{
		if(!world.camera.isActive()) return RaycastHit{};
		return cast(world.camera.getWorldPosition(), world.camera.getFront(), max_distance);
	}
}
