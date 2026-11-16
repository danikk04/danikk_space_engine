#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world_physics.h>
#include "internal.h"
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

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
			WorldBodyId body_id;
			const PlanetComponent* planet;
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

		// Samples the field gradient at mesh points, groups similar normals and resolves support contacts.

		/**
		 * @brief Checks collision for chunk surface with planet.
		 */
		void collideChunkSurfaceWithPlanet(World& world, WorldBody& moving_body,
			const Chunk& moving_chunk, const BodyMassProperties& mass_properties,
			const PlanetCollisionTarget& target)
		{
			if(moving_chunk.ground_triangle_ids.size() == 0) return;
			const WorldBody& planet_body = world.data.getWorldBody(target.body_id);
			const glm::dquat inverse_planet_rotation = glm::conjugate(planet_body.rotation);
			DynamicArray<SurfaceContact> point_contacts;
			point_contacts.reserve(moving_chunk.point_ids.size());
			const auto sample_field = [&](const glm::dvec3& position)
			{
				return samplePlanetScalarField(position, target.planet->radius,
					target.planet->noise_seed, target.planet->terrain_edits, target.planet->core_radius);
			};
			constexpr double gradient_step = planet_voxel_size * 0.25;
			for(PointId point_id : moving_chunk.point_ids)
			{
				if(!world.data.isPointActive(point_id)) continue;
				const glm::dvec3 local_point = moving_chunk.position
					+ glm::dvec3(world.data.getPoint(point_id).position);
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

		// Samples the scalar field and its gradient to find a sphere fallback contact.

		/**
		 * @brief Returns planet surface contact.
		 */
		bool getPlanetSurfaceContact(const PlanetComponent& planet, const glm::dvec3& field_position,
			double collider_radius, glm::dvec3& outward_direction, double& penetration)
		{
			const auto sample_field = [&](const glm::dvec3& position)
			{
				return samplePlanetScalarField(
					position, planet.radius, planet.noise_seed, planet.terrain_edits, planet.core_radius);
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

	// Collects planet targets and resolves sphere or mesh contacts for each moving body.

	/**
	 * @brief Resolves planet collisions.
	 */
	void WorldPhysics::resolvePlanetCollisions(World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("World::planet collisions");
		DynamicArray<PlanetCollisionTarget> planets;

		for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
				planets.push(PlanetCollisionTarget{
					body_id,
					static_cast<const PlanetComponent*>(slot.getData())});
			}
		}
		if(planets.size() == 0) return;

		for(WorldBodyId moving_body_id : world.data.getUsedWorldBodyIds())
		{
			if(!world.data.isWorldBodyActive(moving_body_id)) continue;
			WorldBody& moving_body = world.data.getWorldBody(moving_body_id);
			bool is_planet_body = false;
			for(const PlanetCollisionTarget& target : planets)
			{
				if(target.body_id == moving_body_id)
				{
					is_planet_body = true;
					break;
				}
			}
			if(is_planet_body) continue;
			if(!hasLoadedChunk(world, moving_body)) continue;
			const BodyMassProperties mass_properties = calculateMassProperties(moving_body);
			if(mass_properties.mass <= 0.0) continue;
			bool has_surface_mesh = false;
			for(ChunkId chunk_id : moving_body.chunk_ids)
			{
				if(world.data.isChunkActive(chunk_id)
					&& world.data.getChunk(chunk_id).ground_triangle_ids.size() > 0)
				{
					has_surface_mesh = true;
					break;
				}
			}

			if(moving_body.collision_radius > 0.0 && !has_surface_mesh)
			{
				for(const PlanetCollisionTarget& target : planets)
				{
					if(target.body_id == moving_body_id || !world.data.isWorldBodyActive(target.body_id)) continue;
					const WorldBody& planet_body = world.data.getWorldBody(target.body_id);
					const glm::dvec3 field_position = glm::conjugate(planet_body.rotation)
						* (moving_body.position - planet_body.position);
					glm::dvec3 local_outward(0.0);
					double penetration = 0.0;
					if(!getPlanetSurfaceContact(*target.planet, field_position,
						moving_body.collision_radius, local_outward, penetration)) continue;
					const glm::dvec3 world_outward = planet_body.rotation * local_outward;
					const glm::dvec3 support_point_local = glm::conjugate(moving_body.rotation)
						* (-world_outward * moving_body.collision_radius);
					resolveContact(moving_body, mass_properties, world_outward,
						penetration, support_point_local);
				}
			}

			for(ChunkId chunk_id : moving_body.chunk_ids)
			{
				if(!world.data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = world.data.getChunk(chunk_id);
				for(const PlanetCollisionTarget& target : planets)
				{
					if(target.body_id == moving_body_id) continue;
					collideChunkSurfaceWithPlanet(world, moving_body, chunk, mass_properties, target);
				}
			}
		}
	}
}
