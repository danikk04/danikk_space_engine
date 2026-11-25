#include <cmath>
#include <danikk_space_engine/systems/world_body/system.h>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/physics.h>
#include <danikk_space_engine/systems/physics/internal.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/build_object/system.h>
#include <danikk_space_engine/systems/build_part/system.h>
#include <danikk_space_engine/geometry/build_part.h>

#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/world_body/world_body.h>

namespace danikk_space_engine
{
	using namespace collision_internal;

	namespace
	{

		/**
		 * @brief Pairs a planet body with its settings for collision checks.
		 */
		struct PlanetCollisionTarget
		{
			PlanetId planet_id;
		};

		/**
		 * @brief Stores a candidate contact point, normal, and penetration for a body against terrain.
		 */
		struct SurfaceContact
		{
			glm::dvec3 local_point = glm::dvec3(0.0);
			glm::dvec3 outward_direction = glm::dvec3(0.0, 0.0, 1.0);
			double penetration = 0.0;
		};

		/**
		 * @brief Collects terrain contact candidates and reduces them to the contacts used by the solver.
		 */
		struct SurfaceContactManifold
		{
			SurfaceContact contact;
			uint32 support_point_count = 0;
		};

		/**
		 * @brief Resolves body point and BuildPart geometry penetration into a planet scalar field.
		 * @param world World providing point data and planet/body transforms.
		 * @param moving_body Non-planet body that receives position and velocity corrections.
		 * @param moving_body Body whose indexed points and BuildParts are sampled.
		 * @param mass_properties Approximate mass and scalar inertia for contact impulses.
		 * @param target Planet body and scalar-field settings used for sampling.
		 * For each point inside terrain, estimates the field gradient with central
		 * differences at one quarter of planet_voxel_size. Penetration is density
		 * divided by gradient magnitude. Contacts deeper than two voxel sizes are
		 * discarded; remaining contacts with normals having dot product >= 0.98
		 * are grouped. Four positional and four velocity passes resolve the groups.
		 */
		void collideBodySurfaceWithPlanet(World& world, WorldBodyId moving_body_id,
			WorldBody& moving_body,
			const BodyMassProperties& mass_properties,
			const PlanetCollisionTarget& target)
		{
			const PlanetData& planet = world.getPlanetSystem().getPlanet(target.planet_id);
			const WorldBody& planet_body = world.getWorldBodySystem().getWorldBody(planet.world_body_id);
			const glm::dquat inverse_planet_rotation = glm::conjugate(planet_body.rotation);
			DynamicArray<SurfaceContact> point_contacts;
			const BuildObjectId object_id = world.getBuildObjectSystem().findBuildObjectId(moving_body_id);
			const BuildObject* object = object_id == static_cast<BuildObjectId>(-1)
				? nullptr : &world.getBuildObjectSystem().getBuildObject(object_id);
			const size_t build_part_count = object == nullptr ? 0 : object->build_part_ids.size();
			point_contacts.reserve(moving_body.point_ids.size() + build_part_count * 8);
			const auto sample_field = [&](const glm::dvec3& position)
			{
				return world.getPlanetSystem().getPlanetDensity(target.planet_id, position,
					PlanetTerrainMode::cave_field_with_edits);
			};
			constexpr double gradient_step = planet_voxel_size * 0.25;
			DynamicArray<glm::dvec3> local_points;
			local_points.reserve(moving_body.point_ids.size() + build_part_count * 8);
			for(PointId point_id : moving_body.point_ids)
			{
				if(!world.getPointData().isPointActive(point_id)) continue;
				const Point& point = world.getPointData().getPoint(point_id);
				if(point.world_body_id != moving_body_id) continue;
				local_points.push(point.body_local_position);
			}
			if(object != nullptr)
				for(BuildPartId part_id : object->build_part_ids)
				{
					if(!world.getBuildPartSystem().isBuildPartActive(part_id)) continue;
					const BuildPart& part = world.getBuildPartSystem().getBuildPart(part_id);
					const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
					for(const vec3& vertex : geometry.positions)
						local_points.push(part.body_local_position
							+ glm::dvec3(part.rotation * vec4(vertex * part.size, 0.0f)));
				}
			for(const glm::dvec3& local_point : local_points)
			{
				const glm::dvec3 planet_local_point = inverse_planet_rotation
					* (moving_body.position + moving_body.rotation * local_point - planet_body.position);
				const double density = sample_field(planet_local_point);
				if(density <= 0.0001) continue;
				glm::dvec3 gradient(0.0);
				for(uint8 axis = 0; axis < 3; axis++)
				{
					glm::dvec3 offset(0.0);
					offset[axis] = gradient_step;
					gradient[axis] = (sample_field(planet_local_point + offset)
						- sample_field(planet_local_point - offset)) / (2.0 * gradient_step);
				}
				glm::dvec3 outward_direction = -gradient;
				const double gradient_magnitude = glm::length(outward_direction);
				if(gradient_magnitude <= 0.000001) continue;
				outward_direction /= gradient_magnitude;
				point_contacts.push(SurfaceContact{
					local_point,
					planet_body.rotation * outward_direction,
					density / gradient_magnitude});
			}
			constexpr double maximum_surface_correction = planet_voxel_size * 2.0;
			DynamicArray<SurfaceContactManifold> contact_manifolds;
			for(const SurfaceContact& contact : point_contacts)
			{
				if(contact.penetration <= 0.0
					|| contact.penetration > maximum_surface_correction) continue;
				SurfaceContactManifold* matching_manifold = nullptr;
				for(SurfaceContactManifold& manifold : contact_manifolds)
				{
					if(glm::dot(manifold.contact.outward_direction, contact.outward_direction) >= 0.98)
					{
						matching_manifold = &manifold;
						break;
					}
				}
				if(matching_manifold == nullptr)
				{
					contact_manifolds.push(SurfaceContactManifold{contact, 1});
					continue;
				}
				const double previous_count = matching_manifold->support_point_count;
				const double next_count = previous_count + 1.0;
				matching_manifold->contact.local_point =
					(matching_manifold->contact.local_point * previous_count + contact.local_point) / next_count;
				matching_manifold->contact.outward_direction = glm::normalize(
					matching_manifold->contact.outward_direction * previous_count + contact.outward_direction);
				matching_manifold->contact.penetration = glm::max(
					matching_manifold->contact.penetration, contact.penetration);
				matching_manifold->support_point_count++;
			}

			glm::dvec3 position_correction(0.0);
			for(uint8 iteration = 0; iteration < 4; iteration++)
			{
				for(const SurfaceContactManifold& manifold : contact_manifolds)
				{
					const SurfaceContact& contact = manifold.contact;
					if(contact.penetration <= 0.0
						|| contact.penetration > maximum_surface_correction) continue;
					const double remaining_penetration = contact.penetration
						- glm::dot(position_correction, contact.outward_direction);
					if(remaining_penetration > 0.0001)
					{
						position_correction += contact.outward_direction * remaining_penetration;
					}
				}
			}
			if(glm::dot(position_correction, position_correction) > 0.00000001)
			{
				moving_body.position += position_correction;
			}
			for(uint8 iteration = 0; iteration < 4; iteration++)
			{
				for(const SurfaceContactManifold& manifold : contact_manifolds)
				{
					const SurfaceContact& contact = manifold.contact;
					if(contact.penetration <= 0.0
						|| contact.penetration > maximum_surface_correction) continue;
					applyContactImpulse(moving_body, mass_properties,
						contact.outward_direction, contact.local_point);
				}
			}
		}

		/**
		 * @brief Tests a spherical fallback collider against the planet field and finds an outward normal.
		 * @param planet Planet radius, seed, terrain edits, and core radius.
		 * @param field_position Collider-center position in planet-local coordinates.
		 * @param collider_radius Radius of the spherical fallback collider.
		 * @param outward_direction Receives the normalized field-outward direction.
		 * @param penetration Receives sampled density plus collider radius.
		 * @return False when the resulting penetration is non-positive; true when
		 *         the sphere overlaps the field. The normal is estimated from central
		 *         differences at half a voxel, with radial and fixed-axis fallbacks
		 *         for a degenerate gradient.
		 */
		bool getPlanetSurfaceContact(World& world, PlanetId planet_id,
			const glm::dvec3& field_position,
			double collider_radius, glm::dvec3& outward_direction, double& penetration)
		{
			const auto sample_field = [&](const glm::dvec3& position)
			{
				return world.getPlanetSystem().getPlanetDensity(planet_id, position,
					PlanetTerrainMode::cave_field_with_edits);
			};
			penetration = sample_field(field_position) + collider_radius;
			if(penetration <= 0.0) return false;

			const double gradient_step = planet_voxel_size * 0.5;
			glm::dvec3 gradient(0.0);
			for(uint8 axis = 0; axis < 3; axis++)
			{
				glm::dvec3 offset(0.0);
				offset[axis] = gradient_step;
				gradient[axis] = (sample_field(field_position + offset)
					- sample_field(field_position - offset)) / (2.0 * gradient_step);
			}
			outward_direction = -gradient;
			double direction_length = glm::length(outward_direction);
			if(direction_length <= 0.000001)
			{
				outward_direction = field_position;
				direction_length = glm::length(outward_direction);
			}
			if(direction_length <= 0.000001)
			{
				outward_direction = glm::dvec3(0.0, 0.0, 1.0);
				direction_length = 1.0;
			}
			outward_direction /= direction_length;
			return true;
		}
	}

	/**
		 * @brief Collects planets, then resolves each eligible body's scalar-field contacts.
	 * @param world World containing planet components, point-owned entities, and moving bodies.
	 * A body with a positive collision radius and no active triangle mesh uses the
		 * spherical fallback. Otherwise the body's points and BuildPart vertices are tested.
		 * Planet bodies, unloaded bodies, and bodies with invalid mass
	 * properties are skipped; planets remain static during contact resolution.
	 */
	void PhysicsSystem::resolvePlanetCollisions()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		DynamicArray<PlanetCollisionTarget> planets;

		for(const PlanetData& planet : world.getPlanetSystem().getPlanets())
		{
			planets.push(PlanetCollisionTarget{planet.id});
		}
		if(planets.size() == 0) return;

		for(WorldBodyId moving_body_id : world.getWorldBodySystem().getUsedWorldBodyIds())
		{
			if(!world.getWorldBodySystem().isWorldBodyActive(moving_body_id)) continue;
			WorldBody& moving_body = world.getWorldBodySystem().getWorldBody(moving_body_id);
			if(world.getPlanetSystem().isPlanetWorldBody(moving_body_id)) continue;
			const BuildObjectId build_object_id = world.getBuildObjectSystem().findBuildObjectId(moving_body_id);
			if(moving_body.point_ids.size() == 0
				&& (build_object_id == static_cast<BuildObjectId>(-1)
					|| world.getBuildObjectSystem().getBuildObject(build_object_id).build_part_ids.size() == 0)) continue;
			const BodyMassProperties mass_properties = calculateMassProperties(moving_body);
			if(mass_properties.mass <= 0.0) continue;
			const bool has_surface_geometry = build_object_id != static_cast<BuildObjectId>(-1)
				&& world.getBuildObjectSystem().getBuildObject(build_object_id).build_part_ids.size() > 0;

			if(moving_body.collision_radius > 0.0 && !has_surface_geometry)
			{
				for(const PlanetCollisionTarget& target : planets)
				{
					const PlanetData& planet = world.getPlanetSystem().getPlanet(target.planet_id);
					const WorldBody& planet_body = world.getWorldBodySystem().getWorldBody(planet.world_body_id);
					const glm::dvec3 field_position = glm::conjugate(planet_body.rotation)
						* (moving_body.position - planet_body.position);
					glm::dvec3 local_outward(0.0);
					double penetration = 0.0;
					if(!getPlanetSurfaceContact(world, target.planet_id, field_position,
						moving_body.collision_radius, local_outward, penetration)) continue;
					const glm::dvec3 world_outward = planet_body.rotation * local_outward;
					const glm::dvec3 support_point_local = glm::conjugate(moving_body.rotation)
						* (-world_outward * moving_body.collision_radius);
					resolveContact(moving_body, mass_properties, world_outward,
						penetration, support_point_local);
				}
			}

			for(const PlanetCollisionTarget& target : planets)
				if(world.getPlanetSystem().getPlanet(target.planet_id).world_body_id != moving_body_id)
					collideBodySurfaceWithPlanet(world, moving_body_id, moving_body, mass_properties, target);
		}
	}
}
