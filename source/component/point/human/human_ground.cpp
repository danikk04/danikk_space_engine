#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/item/item.h>
#include <danikk_space_engine/world.h>

#include "internal.h"

namespace danikk_space_engine::human_internal
{
		constexpr float minimum_ground_normal_dot = 0.2f;

		/**
		 * @brief Finds a supported character-center position by sampling the planet signed field.
		 *
		 * Brackets the density transition along `up`, bisects it, estimates the surface normal
		 * from a finite-difference gradient, and rejects contacts steeper than the walkable limit.
		 * @param body_id WorldBody owning the human point.
		 * @param planet Scalar-field parameters and terrain edit stamps.
		 * @param up Character's local up direction.
		 * @param human_height Character capsule height.
		 * @param proposed Candidate character-center position in body-local coordinates.
		 * @param supported Receives the adjusted center position on valid support.
		 * @param step_height Maximum upward step probe distance.
		 * @param maximum_drop Maximum downward support probe distance.
		 * @return True if a walkable surface supports the character within the probe interval.
		 */
		bool findPlanetGroundPosition(World& world, WorldBodyId planet_body_id,
			const vec3& up, float human_height,
			const glm::dvec3& proposed, glm::dvec3& supported, vec3& surface_normal,
			float step_height, float maximum_drop)
		{
			DSE_WATCH();
			const double capsule_radius = human_height * 0.14;
			const double half_axis = human_height * 0.5 - capsule_radius;
			const glm::dvec3 bottom_center = proposed - glm::dvec3(up * static_cast<float>(half_axis));
			const auto densityAtOffset = [&](double offset)
			{
				return world.planets.getPlanetDensity(planet_body_id,
					bottom_center + glm::dvec3(up) * offset,
					PlanetTerrainMode::cave_field_with_edits);
			};
			double upper = step_height;
			double lower = -maximum_drop;
			double upper_density = densityAtOffset(upper);
			// Physics corrections or terrain edits can leave the capsule slightly inside the field.
			// Find a nearby air sample before bisecting instead of treating penetration as free fall.
			constexpr double max_penetration_recovery = 2.0;
			while(upper < max_penetration_recovery && upper_density > -capsule_radius)
			{
				upper = glm::min(upper + 0.25, max_penetration_recovery);
				upper_density = densityAtOffset(upper);
			}
			if(upper_density > -capsule_radius || densityAtOffset(lower) < -capsule_radius) return false;
			for(uint8 iteration = 0; iteration < 8; iteration++)
			{
				const double middle = (upper + lower) * 0.5;
				if(densityAtOffset(middle) < -capsule_radius) upper = middle;
				else lower = middle;
			}
			const glm::dvec3 contact_position = bottom_center + glm::dvec3(up) * ((upper + lower) * 0.5);
			constexpr double gradient_step = planet_voxel_size * 0.25;
			glm::dvec3 gradient(0.0);
			for(uint8 axis = 0; axis < 3; axis++)
			{
				glm::dvec3 offset(0.0);
				offset[axis] = gradient_step;
				gradient[axis] = (world.planets.getPlanetDensity(planet_body_id,
					contact_position + offset, PlanetTerrainMode::cave_field_with_edits)
					- world.planets.getPlanetDensity(planet_body_id,
						contact_position - offset, PlanetTerrainMode::cave_field_with_edits))
					/ (2.0 * gradient_step);
			}
			const double gradient_length = glm::length(gradient);
			if(gradient_length <= 1e-8 || glm::dot(-gradient / gradient_length, glm::dvec3(up))
				< minimum_ground_normal_dot) return false;
			surface_normal = vec3(-gradient / gradient_length);
			const glm::dvec3 supported_bottom = contact_position + glm::dvec3(up) * 0.02;
			supported = supported_bottom + glm::dvec3(up * static_cast<float>(half_axis));
			return true;
		}

		/**
		 * @brief Finds a walkable support position using the planet field or a raycast on the owning body.
		 * @param world World used for scalar-field data and raycast queries.
		 * @param body_id WorldBody owning the character.
		 * @param body Body whose terrain or mesh surfaces can support the character.
		 * @param planet The already resolved planet component, or null for mesh support.
		 * @param up Character-local up direction.
		 * @param human_height Character height used to locate the feet.
		 * @param proposed Candidate center position in body-local coordinates.
		 * @param supported Receives the corrected center position when support is found.
		 * @param step_height Maximum upward step distance to test.
		 * @param maximum_drop Maximum downward distance to search for support.
		 * @return True when a sufficiently upward-facing surface is found within the interval.
		 */
		bool findGroundPosition(World& world, WorldBodyId body_id, const WorldBody& body,
			const PlanetData* planet,
			const vec3& up, float human_height, const glm::dvec3& proposed, glm::dvec3& supported,
			vec3& surface_normal, float step_height = 0.5f, float maximum_drop = 0.75f)
		{
			DSE_WATCH();
			if(planet != nullptr)
				return findPlanetGroundPosition(world, body_id, up, human_height,
					proposed, supported, surface_normal, step_height, maximum_drop);

			const glm::dvec3 proposed_foot = proposed - glm::dvec3(up * (human_height * 0.5f));
			const glm::dvec3 foot = body.position
				+ body.rotation * proposed_foot;
			const vec3 world_up = vec3(body.rotation * glm::dvec3(up));
			const RaycastHit hit = world.raycasts.castBody(body_id,
				foot + glm::dvec3(world_up) * static_cast<double>(step_height),
				-world_up, step_height + maximum_drop);
			if(!hit.hit || glm::dot(hit.normal, world_up) < minimum_ground_normal_dot) return false;
			surface_normal = vec3(glm::normalize(glm::conjugate(body.rotation) * glm::dvec3(hit.normal)));
			const glm::dvec3 supported_foot = glm::conjugate(body.rotation)
				* (hit.position - body.position) + glm::dvec3(up * 0.02f);
			supported = supported_foot + glm::dvec3(up * (human_height * 0.5f));
			return true;
		}

		/**
		 * @brief Applies tangential movement while supported, or integrates and sweeps a fall.
		 *
		 * When ground following is disabled, movement is unconstrained and vertical speed is reset.
		 * Confirmed planet support can be reused while the capsule and terrain revision match.
		 * A nonzero step probes its destination. Without support, the helper applies gravity
		 * and checks the full downward travel before landing.
		 * @param world World used by support queries.
		 * @param body_id Owning body used for local/world coordinate conversion.
		 * @param body Owning world body and transform.
		 * @param human Character dimensions and grounded/fall state.
		 * @param point Mutable point position representing the character center.
		 * @param velocity Tangential movement command for this tick.
		 * @param gravity Gravity magnitude in units per second squared.
		 * @param dt Simulation step duration in seconds.
		 */
		void moveWithGround(World& world, WorldBodyId body_id, const WorldBody& body,
			HumanComponent& human, Point& point, const vec3& velocity, float gravity, float dt)
		{
			DSE_WATCH();
			const glm::dvec3 step = glm::dvec3(velocity) * static_cast<double>(dt);
			auto& position = point.body_local_position;
			auto& cache = human.ground_support;
			if(!human.follow_ground)
			{
				position += step;
				human.vertical_speed = 0.0f;
				human.grounded = false;
				return;
			}

			const PlanetData* planet = world.planets.findPlanet(body_id);
			const float height = human.getHeight();
			const bool cached_support = planet != nullptr && human.grounded && human.vertical_speed == 0.0f
				&& cache.planet_generation_id == planet->generation_id
				&& cache.terrain_revision == planet->terrain_revision && cache.height == height
				&& cache.up == human.up && cache.center == position;

			if(cached_support && position + step == position) return;

			const auto setGrounded = [&](const vec3& supported_position, const vec3& surface_normal)
			{
				position = supported_position;
				human.ground_normal = glm::length(surface_normal) > 1e-6f
					? glm::normalize(surface_normal) : human.up;
				human.vertical_speed = 0.0f;
				human.grounded = true;
				cache.center = position;
				cache.up = human.up;
				cache.height = height;
				cache.planet_generation_id = planet == nullptr ? 0 : planet->generation_id;
				cache.terrain_revision = planet == nullptr ? 0 : planet->terrain_revision;
			};

			glm::dvec3 support;
			vec3 support_normal = human.ground_normal;
			glm::dvec3 supported_position = position;
			bool has_support = cached_support;
			if(!has_support && human.vertical_speed <= 0.0f
				&& findGroundPosition(world, body_id, body, planet, human.up, height,
					position, support, support_normal))
			{
				const double lift = glm::dot(support - position, glm::dvec3(human.up));
				has_support = lift >= -0.08f && lift <= 2.0f;
				if(has_support) supported_position = support;
			}

			if(has_support)
			{
				const glm::dvec3 proposed = supported_position + step;
				if(proposed != supported_position
					&& findGroundPosition(world, body_id, body, planet, human.up, height,
						proposed, support, support_normal))
					supported_position = support;
				setGrounded(supported_position, support_normal);
				return;
			}

			human.grounded = false;
			const float vertical_displacement = human.vertical_speed * dt - 0.5f * gravity * dt * dt;
			human.vertical_speed -= gravity * dt;
			if(vertical_displacement <= 0.0f
				&& findGroundPosition(world, body_id, body, planet, human.up, height, position,
					support, support_normal, 0.0f, -vertical_displacement + 0.02f))
			{
				setGrounded(support, support_normal);
			}
			else
			{
				position += glm::dvec3(human.up * vertical_displacement);
			}
		}

}
