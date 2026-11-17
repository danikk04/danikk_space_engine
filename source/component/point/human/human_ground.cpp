#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/item.h>
#include <danikk_space_engine/world/world.h>

#include "internal.h"

namespace danikk_space_engine::human_internal
{
		constexpr float minimum_ground_normal_dot = 0.2f;

		/**
		 * @brief Finds the planet component attached to a world body, if any.
		 * @param data World resource pools used to validate component-slot IDs.
		 * @param body World body whose component slots are inspected.
		 * @return Planet payload for the first planet component, or null for non-planet bodies.
		 */
		const PlanetComponent* getPlanet(const WorldData& data, const WorldBody& body)
		{
			Watcher watcher("getPlanet");
			for(WorldBodyComponentSlotId component_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(component_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(component_id);
				if(slot.type_id == WorldBodyComponentTypeId::planet)
					return static_cast<const PlanetComponent*>(slot.getData());
			}
			return nullptr;
		}

		/**
		 * @brief Finds a supported character-center position by sampling the planet signed field.
		 *
		 * Brackets the density transition along `up`, bisects it, estimates the surface normal
		 * from a finite-difference gradient, and rejects contacts steeper than the walkable limit.
		 * @param chunk Chunk containing the human point.
		 * @param body Owning world body; used for the planet-relative transform context.
		 * @param planet Scalar-field parameters and terrain edit stamps.
		 * @param up Character's local up direction.
		 * @param human_height Character capsule height.
		 * @param proposed Candidate character-center position in chunk-local coordinates.
		 * @param supported Receives the adjusted center position on valid support.
		 * @param step_height Maximum upward step probe distance.
		 * @param maximum_drop Maximum downward support probe distance.
		 * @return True if a walkable surface supports the character within the probe interval.
		 */
		bool findPlanetGroundPosition(const Chunk& chunk, const WorldBody& body,
			const PlanetComponent& planet, const vec3& up, float human_height,
			const vec3& proposed, vec3& supported, float step_height, float maximum_drop)
		{
			Watcher watcher("findPlanetGroundPosition");
			const double capsule_radius = human_height * 0.14;
			const double half_axis = human_height * 0.5 - capsule_radius;
			const glm::dvec3 bottom_center = chunk.position
				+ glm::dvec3(proposed - up * static_cast<float>(half_axis));
			const auto densityAtOffset = [&](double offset)
			{
				return samplePlanetScalarField(bottom_center + glm::dvec3(up) * offset,
					planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius);
			};
			double upper = step_height;
			double lower = -maximum_drop;
			// Physics corrections or terrain edits can leave the capsule slightly inside the field.
			// Find a nearby air sample before bisecting instead of treating penetration as free fall.
			constexpr double max_penetration_recovery = 2.0;
			while(upper < max_penetration_recovery && densityAtOffset(upper) > -capsule_radius)
				upper = glm::min(upper + 0.25, max_penetration_recovery);
			if(densityAtOffset(upper) > -capsule_radius || densityAtOffset(lower) < -capsule_radius) return false;
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
				gradient[axis] = (samplePlanetScalarField(contact_position + offset,
					planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius)
					- samplePlanetScalarField(contact_position - offset,
						planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius))
					/ (2.0 * gradient_step);
			}
			const double gradient_length = glm::length(gradient);
			if(gradient_length <= 1e-8 || glm::dot(-gradient / gradient_length, glm::dvec3(up))
				< minimum_ground_normal_dot) return false;
			const glm::dvec3 supported_bottom = contact_position + glm::dvec3(up) * 0.02;
			supported = vec3(supported_bottom - chunk.position) + up * static_cast<float>(half_axis);
			(void)body;
			return true;
		}

		/**
		 * @brief Finds a walkable support position using scalar-field sampling or world raycast.
		 * @param world World used for scalar-field data and raycast queries.
		 * @param chunk Chunk containing the character.
		 * @param body Body whose terrain or mesh surfaces can support the character.
		 * @param up Character-local up direction.
		 * @param human_height Character height used to locate the feet.
		 * @param proposed Candidate center position in chunk-local coordinates.
		 * @param supported Receives the corrected center position when support is found.
		 * @param step_height Maximum upward step distance to test.
		 * @param maximum_drop Maximum downward distance to search for support.
		 * @return True when a sufficiently upward-facing surface is found within the interval.
		 */
		bool findGroundPosition(World& world, const Chunk& chunk, const WorldBody& body,
			const vec3& up, float human_height, const vec3& proposed, vec3& supported,
			float step_height = 0.5f, float maximum_drop = 0.75f)
		{
			Watcher watcher("findGroundPosition");
			if(const PlanetComponent* planet = getPlanet(world.data, body))
				return findPlanetGroundPosition(chunk, body, *planet, up, human_height,
					proposed, supported, step_height, maximum_drop);

			const vec3 proposed_foot = proposed - up * (human_height * 0.5f);
			const glm::dvec3 foot = body.position
				+ body.rotation * (chunk.position + glm::dvec3(proposed_foot));
			const vec3 world_up = vec3(body.rotation * glm::dvec3(up));
			const RaycastHit hit = world.raycast(foot + glm::dvec3(world_up) * static_cast<double>(step_height),
				-world_up, step_height + maximum_drop);
			if(!hit.hit || hit.body_id != chunk.world_body_id
				|| glm::dot(hit.normal, world_up) < minimum_ground_normal_dot) return false;
			const vec3 supported_foot = vec3(glm::conjugate(body.rotation)
				* (hit.position - body.position) - chunk.position) + up * 0.02f;
			supported = supported_foot + up * (human_height * 0.5f);
			return true;
		}

		/**
		 * @brief Applies tangential movement while supported, or integrates and sweeps a fall.
		 *
		 * When ground following is disabled, movement is unconstrained and vertical speed is reset.
		 * Otherwise the helper keeps the character on nearby support, limits step/drop probes,
		 * applies gravity after support loss, and checks the full downward travel before landing.
		 * @param world World used by support queries.
		 * @param chunk Owning chunk used for local/world coordinate conversion.
		 * @param body Owning world body and transform.
		 * @param human Character dimensions and grounded/fall state.
		 * @param point Mutable point position representing the character center.
		 * @param velocity Tangential movement command for this tick.
		 * @param gravity Gravity magnitude in units per second squared.
		 * @param dt Simulation step duration in seconds.
		 */
		void moveWithGround(World& world, const Chunk& chunk, const WorldBody& body,
			HumanComponent& human, Point& point, const vec3& velocity, float gravity, float dt)
		{
			Watcher watcher("moveWithGround");
			if(!human.follow_ground)
			{
				point.position += velocity * dt;
				human.vertical_speed = 0.0f;
				human.grounded = false;
				return;
			}
			vec3 support;
			if(human.vertical_speed <= 0.0f
				&& findGroundPosition(world, chunk, body, human.up, human.getHeight(), point.position, support)
				&& glm::dot(support - point.position, human.up) >= -0.08f
				&& glm::dot(support - point.position, human.up) <= 2.0f)
			{
				point.position = support;
				if(findGroundPosition(world, chunk, body, human.up, human.getHeight(),
					point.position + velocity * dt, support))
					point.position = support;
				human.vertical_speed = 0.0f;
				human.grounded = true;
				return;
			}

			human.grounded = false;
			const float vertical_displacement = human.vertical_speed * dt - 0.5f * gravity * dt * dt;
			human.vertical_speed -= gravity * dt;
			if(vertical_displacement <= 0.0f
				&& findGroundPosition(world, chunk, body, human.up, human.getHeight(), point.position, support,
					0.0f, -vertical_displacement + 0.02f))
			{
				point.position = support;
				human.vertical_speed = 0.0f;
				human.grounded = true;
			}
			else
			{
				point.position += human.up * vertical_displacement;
			}
		}

}
