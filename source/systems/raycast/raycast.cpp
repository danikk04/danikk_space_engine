#include <cmath>
#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_framework/glm.h>

#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/raycast/raycast.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/math/ray.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/geometry/build_part.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	void RaycastSystem::init() { danikk_framework::formatLogInfo("Init RaycastSystem"); }
	void RaycastSystem::shutdown() { danikk_framework::formatLogInfo("Shutdown RaycastSystem"); }
	namespace
	{

		bool raySphereInterval(const glm::dvec3& center_from_origin, double radius,
			const glm::dvec3& direction, double max_distance, double& interval_start, double& interval_end)
		{
			const double projection = glm::dot(center_from_origin, direction);
			const double perpendicular_squared = glm::dot(center_from_origin, center_from_origin)
				- projection * projection;
			const double radius_squared = radius * radius;
			if(perpendicular_squared > radius_squared) return false;

			const double half_chord = std::sqrt(glm::max(0.0, radius_squared - perpendicular_squared));
			const double entry = projection - half_chord;
			const double exit = projection + half_chord;
			if(exit < 0.0 || entry > max_distance) return false;
			interval_start = glm::max(0.0, entry);
			interval_end = glm::min(max_distance, exit);
			return interval_start <= interval_end;
		}

	}

	RaycastHit RaycastSystem::cast(const glm::dvec3& origin, const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		const auto& body_ids = world.getWorldBodyData().getUsedWorldBodyIds();
		RaycastHit result = castBodies(body_ids.begin(), body_ids.size(), origin, direction, max_distance);
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12 || max_distance < 0.0) return result;
		const glm::dvec3 ray_direction = glm::dvec3(direction) / direction_length;
		float nearest_distance = result.hit ? result.distance : max_distance;
		for(const PlanetData& planet : world.getPlanetSystem().getPlanets())
		{
			bool has_proxy = false;
			for(WorldBodyId body_id : body_ids)
				if(world.getWorldBodyData().getWorldBody(body_id).planet_id == planet.id)
				{
					has_proxy = true;
					break;
				}
			if(has_proxy) continue;
			const PlanetSettings& settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
			double broadphase_radius = settings.radius + 11000.0;
			for(PlanetTerrainEditId edit_id : planet.terrain_edits.ids)
			{
				if(!planet.terrain_edits.pool->isActive(edit_id)) continue;
				broadphase_radius = glm::max(broadphase_radius,
					(*planet.terrain_edits.pool)[edit_id].getMaximumRadiusFromOrigin(1.0));
			}
			double interval_start = 0.0;
			double interval_end = 0.0;
			if(!raySphereInterval(planet.position - origin, broadphase_radius,
				ray_direction, nearest_distance, interval_start, interval_end)) continue;
			const glm::dquat inverse_rotation = glm::conjugate(planet.rotation);
			const glm::dvec3 local_direction = inverse_rotation * ray_direction;
			const glm::dvec3 local_origin = inverse_rotation
				* (origin + ray_direction * interval_start - planet.position);
			double field_distance = 0.0;
			glm::dvec3 field_normal;
			if(!raycastPlanetScalarField(planet.id, local_origin, local_direction,
				interval_end - interval_start, PlanetTerrainMode::cave_field_with_edits,
				field_distance, field_normal)) continue;
			const double hit_distance = interval_start + field_distance;
			if(hit_distance >= nearest_distance) continue;
			nearest_distance = static_cast<float>(hit_distance);
			result = RaycastHit{};
			result.hit = true;
			result.hit_terrain = true;
			result.distance = nearest_distance;
			result.position = origin + ray_direction * hit_distance;
			result.normal = vec3(planet.rotation * field_normal);
			result.planet_id = planet.id;
			for(WorldBodyId body_id : body_ids)
				if(world.getWorldBodyData().getWorldBody(body_id).planet_id == planet.id)
				{
					result.body_id = body_id;
					break;
				}
		}
		return result;
	}

	RaycastHit RaycastSystem::castBody(WorldBodyId body_id, const glm::dvec3& origin,
		const vec3& direction, float max_distance) const
	{
		DSE_WATCH();
		if(!world.getWorldBodyData().isWorldBodyActive(body_id)) return RaycastHit{};
		return castBodies(&body_id, 1, origin, direction, max_distance);
	}

	/** @brief Finds a sign change in the planet density field and estimates its surface normal. */
	bool RaycastSystem::raycastPlanetScalarField(PlanetId planet_id,
		const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
		PlanetTerrainMode mode, double& distance, glm::dvec3& normal) const
	{
		DSE_WATCH();
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12 || max_distance < 0.0) return false;
		const glm::dvec3 ray_direction = direction / direction_length;
		const auto densityAt = [&](double offset)
		{
			return world.getPlanetSystem().getPlanetDensity(planet_id,
				origin + ray_direction * offset, mode);
		};

		const auto normalAt = [&](const glm::dvec3& position, glm::dvec3& result_normal)
		{
			constexpr double gradient_step = 0.05;
			glm::dvec3 gradient(0.0);
			for(uint8 axis = 0; axis < 3; ++axis)
			{
				glm::dvec3 offset(0.0);
				offset[axis] = gradient_step;
				gradient[axis] = world.getPlanetSystem().getPlanetDensity(planet_id,
					position + offset, mode) - world.getPlanetSystem().getPlanetDensity(
						planet_id, position - offset, mode);
			}
			const double gradient_length = glm::length(gradient);
			if(gradient_length <= 1e-12) return false;
			result_normal = -gradient / gradient_length;
			return true;
		};
		constexpr double step = 0.5;
		double previous_distance = 0.0;
		double previous_density = densityAt(0.0);
		if(previous_density > 0.0)
		{
			distance = 0.0;
			return normalAt(origin, normal);
		}
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
				if(!normalAt(hit_position, normal)) return false;
				distance = hit_distance;
				return true;
			}
			previous_distance = next_distance;
			previous_density = next_density;
		}
		return false;
	}

	/**
		 * @brief Traverses octree point candidates and records the nearest scalar-field or BuildPart hit.
	 *
	 * Planet bodies use a sphere test followed by 3D DDA through crossed grid cells. Other
		 * bodies scan their point IDs. Planet terrain is sampled from the scalar field; BuildParts
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
		const WorldBodyData& world_body_data = owner.getWorldBodyData();
		const PlanetSystem& planets = owner.getPlanetSystem();
		const PointData& point_data = owner.getPointData();
		RaycastHit result;
		const float direction_length = glm::length(direction);
		if(direction_length <= 0.000001f || max_distance < 0.0f) return result;

		const vec3 normalized_direction = direction / direction_length;
		float nearest_distance = max_distance;

		for(size_t body_index = 0; body_index < body_count; ++body_index)
		{
			const WorldBodyId body_id = body_ids[body_index];
			const WorldBody& body = world_body_data.getWorldBody(body_id);
			const bool is_planet = body.planet_id != static_cast<PlanetId>(-1);
			const PlanetData* planet = is_planet ? planets.findPlanet(body.planet_id) : nullptr;
			if(is_planet && planet == nullptr) continue;
			const glm::dquat body_rotation = is_planet ? planet->rotation : body.rotation;
			const glm::dvec3 body_position = is_planet ? planet->position : body.position;
			const glm::dquat inverse_rotation = glm::conjugate(body_rotation);
			const glm::dvec3 local_origin = inverse_rotation * (origin - body_position);
			const vec3 local_direction = vec3(inverse_rotation * glm::dvec3(normalized_direction));
			auto test_point = [&](PointId point_id)
			{
				if(!point_data.isPointActive(point_id)) return;
				const Point& point = point_data.getPoint(point_id);
				if(point.world_body_id != body_id) return;
				for(PointComponentSlotId slot_id : point.component_slot_ids)
				{
					if(!point_data.isPointComponentSlotActive(slot_id)) continue;
					const PointComponentSlot& slot = point_data.getPointComponentSlot(slot_id);
					if(slot.type_id != PointComponentTypeId::build_part) continue;
					const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
					const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
					const glm::dvec3 anchor = point.body_local_position;
					const Ray part_local_ray{vec3(local_origin - anchor), local_direction};
					for(const CubeTriangle& triangle : geometry.triangles)
					{
						const vec3 a = vec3(part.rotation * vec4(geometry.positions[triangle.points[0]] * part.size, 0.0f));
						const vec3 b = vec3(part.rotation * vec4(geometry.positions[triangle.points[1]] * part.size, 0.0f));
						const vec3 c = vec3(part.rotation * vec4(geometry.positions[triangle.points[2]] * part.size, 0.0f));
						const RayTriangleIntersection intersection = intersectRayTriangle(part_local_ray, a, b, c);
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
						result.planet_id = planet->id;
					}
				}
			};

			if(is_planet)
			{
				const PlanetSettings& planet_settings = planets.getPlanetSettings(planet->settings_id);
				// Bound the full procedural height range and any additive terrain edits.
				// Terrain queries must not depend on whether a rendered terrain is currently loaded.
				double broadphase_radius = planet_settings.radius + 11000.0;
				for(PlanetTerrainEditId edit_id : planet->terrain_edits.ids)
				{
					if(!planet->terrain_edits.pool->isActive(edit_id)) continue;
					broadphase_radius = glm::max(broadphase_radius,
						(*planet->terrain_edits.pool)[edit_id].getMaximumRadiusFromOrigin(1.0));
				}
				double terrain_interval_start = 0.0;
				double terrain_interval_end = 0.0;
				if(raySphereInterval(body_position - origin, broadphase_radius,
					glm::dvec3(normalized_direction), nearest_distance,
					terrain_interval_start, terrain_interval_end))
				{
					double field_distance = 0.0;
					glm::dvec3 field_normal;
					const glm::dvec3 terrain_ray_origin = origin
						+ glm::dvec3(normalized_direction) * terrain_interval_start;
					if(raycastPlanetScalarField(planet->id,
						inverse_rotation * (terrain_ray_origin - body_position),
						glm::dvec3(local_direction), terrain_interval_end - terrain_interval_start,
						PlanetTerrainMode::cave_field_with_edits, field_distance, field_normal))
					{
						const double hit_distance = terrain_interval_start + field_distance;
						nearest_distance = static_cast<float>(hit_distance);
						result = RaycastHit{};
						result.hit = true;
						result.hit_terrain = true;
						result.distance = nearest_distance;
						result.position = origin + glm::dvec3(normalized_direction) * hit_distance;
						result.normal = vec3(body_rotation * field_normal);
						result.body_id = body_id;
						result.planet_id = planet->id;
					}
				}
			}
			DynamicArray<PointId> candidates;
			body.spatial_tree.collectRayCandidates(local_origin, glm::dvec3(local_direction),
				nearest_distance, candidates);
			for(PointId point_id : candidates) test_point(point_id);
		}
		return result;
	}

	/**
	 * @brief Uses the active camera position and forward vector as the ray origin and direction.
	 * @param max_distance Maximum accepted ray length.
	 * @return Nearest hit, or a miss if the camera's WorldBody is not active.
	 */
	RaycastHit RaycastSystem::castFromCamera(float max_distance) const
	{
		if(!world.getCameraSystem().isActive()) return RaycastHit{};
		return cast(world.getCameraSystem().getWorldPosition(), world.getCameraSystem().getFront(), max_distance);
	}
}
