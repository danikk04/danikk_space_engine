#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_engine/danikk_engine.h>

namespace danikk_space_engine
{
	namespace
	{
		struct PlanetCollisionTarget
		{
			WorldBodyId body_id;
			const PlanetComponent* planet;
		};

		struct SurfaceContact
		{
			glm::dvec3 local_point = glm::dvec3(0.0);
			glm::dvec3 outward_direction = glm::dvec3(0.0, 0.0, 1.0);
			double penetration = 0.0;
		};

		struct SurfaceContactManifold
		{
			SurfaceContact contact;
			uint32 support_point_count = 0;
		};

		struct BodyMassProperties
		{
			double mass = 0.0;
			glm::dvec3 local_center_of_mass = glm::dvec3(0.0);
			double scalar_inertia = 0.0;
		};

		void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& outward_direction, double penetration,
			const glm::dvec3& support_point_local);
		void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& outward_direction, const glm::dvec3& support_point_local);

		/*
		 * Resolves a moving mesh against the planet's continuous scalar field rather than its
		 * triangulated render mesh. The field gradient gives contact normals and density divided by
		 * gradient magnitude approximates penetration depth. Nearly parallel vertex contacts are
		 * merged into support manifolds before position and impulse solving.
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
					target.planet->noise_seed, target.planet->terrain_edits);
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

		BodyMassProperties calculateMassProperties(const WorldBody& body)
		{
			BodyMassProperties properties;
			if(body.collision_radius <= 0.0) return properties;
			properties.mass = body.mass > 0.0 ? body.mass : 1.0;
			properties.scalar_inertia = glm::max(
				0.4 * properties.mass * body.collision_radius * body.collision_radius,
				0.000001);
			properties.scalar_inertia = glm::max(properties.scalar_inertia, 0.000001);
			return properties;
		}

		void applyImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& contact_offset, const glm::dvec3& impulse);

		bool getPlanetSurfaceContact(const PlanetComponent& planet, const glm::dvec3& field_position,
			double collider_radius, glm::dvec3& outward_direction, double& penetration)
		{
			const auto sample_field = [&](const glm::dvec3& position)
			{
				return samplePlanetScalarField(position, planet.radius, planet.noise_seed, planet.terrain_edits);
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

		void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& outward_direction, const glm::dvec3& support_point_local)
		{
			const glm::dvec3 world_center_of_mass = body.position
				+ body.rotation * mass_properties.local_center_of_mass;
			const glm::dvec3 contact_point = body.position + body.rotation * support_point_local;
			const glm::dvec3 contact_offset = contact_point - world_center_of_mass;
			const double inverse_mass = 1.0 / mass_properties.mass;
			const double inverse_inertia = 1.0 / mass_properties.scalar_inertia;
			const glm::dvec3 contact_velocity = body.linear_velocity
				+ glm::cross(body.angular_velocity, contact_offset);
			const double inward_speed = glm::dot(contact_velocity, outward_direction);
			if(inward_speed >= 0.0) return;

			const glm::dvec3 normal_arm = glm::cross(contact_offset, outward_direction);
			const double normal_effective_mass = inverse_mass
				+ glm::dot(normal_arm, normal_arm) * inverse_inertia;
			const double normal_impulse_magnitude = -inward_speed / normal_effective_mass;
			applyImpulse(body, mass_properties, contact_offset,
				outward_direction * normal_impulse_magnitude);

			const glm::dvec3 post_normal_velocity = body.linear_velocity
				+ glm::cross(body.angular_velocity, contact_offset);
			const glm::dvec3 tangent_velocity = post_normal_velocity
				- outward_direction * glm::dot(post_normal_velocity, outward_direction);
			const double tangent_speed = glm::length(tangent_velocity);
			if(tangent_speed <= 0.000001) return;
			const glm::dvec3 tangent_direction = tangent_velocity / tangent_speed;
			const glm::dvec3 tangent_arm = glm::cross(contact_offset, tangent_direction);
			const double tangent_effective_mass = inverse_mass
				+ glm::dot(tangent_arm, tangent_arm) * inverse_inertia;
			constexpr double friction_coefficient = 0.9;
			const double friction_impulse_magnitude = glm::clamp(
				-tangent_speed / tangent_effective_mass,
				-friction_coefficient * normal_impulse_magnitude,
				friction_coefficient * normal_impulse_magnitude);
			applyImpulse(body, mass_properties, contact_offset,
				tangent_direction * friction_impulse_magnitude);
		}

		void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& outward_direction, double penetration,
			const glm::dvec3& support_point_local)
		{
			body.position += outward_direction * (penetration + 0.001);
			applyContactImpulse(body, mass_properties, outward_direction, support_point_local);
		}

		void applyImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& contact_offset, const glm::dvec3& impulse)
		{
			body.linear_velocity += impulse / mass_properties.mass;
			body.angular_velocity += glm::cross(contact_offset, impulse) / mass_properties.scalar_inertia;
		}

		WorldBodyId getDraggedWorldBodyId(const World& world)
		{
			if(world.local_player_controller_component_slot_id >= world.data.getPointComponentSlotCount()
				|| !world.data.isPointComponentSlotActive(world.local_player_controller_component_slot_id))
			{
				return static_cast<WorldBodyId>(-1);
			}
			const PointComponentSlot& slot = world.data.getPointComponentSlot(
				world.local_player_controller_component_slot_id);
			return static_cast<const LocalPlayerControllerComponent*>(slot.getData())->dragged_world_body_id;
		}
	}

	// Applies each planet's gravity to every other non-planet body and integrates linear velocity
	// with the engine's fixed frame step. Acceleration is 9.8 m/s^2 at the surface and follows an
	// inverse-square falloff outside the planet; bodies inside use a linear falloff toward the core.
	void World::integrateWorldBodyMotion()
	{
		Watcher watcher("World::body motion");
		const float delta_time = danikk_engine::getTargetFrameDelay();
		if(delta_time <= 0.0f) return;

		DynamicArray<PlanetCollisionTarget> planets;
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
				planets.push(PlanetCollisionTarget{
					body_id,
					static_cast<const PlanetComponent*>(slot.getData())});
			}
		}

		const WorldBodyId dragged_body_id = getDraggedWorldBodyId(*this);
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			if(body_id == dragged_body_id) continue;
			bool is_planet = false;
			for(const PlanetCollisionTarget& target : planets)
			{
				if(target.body_id == body_id)
				{
					is_planet = true;
					break;
				}
			}
			if(is_planet) continue;

			WorldBody& body = data.getWorldBody(body_id);
			glm::dvec3 acceleration(0.0);
			for(const PlanetCollisionTarget& target : planets)
			{
				if(target.body_id == body_id || target.planet->surface_gravity <= 0.0
					|| target.planet->radius <= 0.0) continue;

				const glm::dvec3 toward_planet = data.getWorldBody(target.body_id).position - body.position;
				const double distance = glm::length(toward_planet);
				if(distance <= 0.000001) continue;
				const double radius = target.planet->radius;
				const double distance_ratio = radius / distance;
				const double gravity = distance > radius
					? target.planet->surface_gravity * distance_ratio * distance_ratio
					: target.planet->surface_gravity * distance_ratio;
				acceleration += toward_planet / distance * gravity;
			}

			body.linear_velocity += acceleration * static_cast<double>(delta_time);
			body.position += body.linear_velocity * static_cast<double>(delta_time);

			const double angular_speed = glm::length(body.angular_velocity);
			if(angular_speed > 0.000001)
			{
				const glm::dquat rotation_delta = glm::angleAxis(
					angular_speed * static_cast<double>(delta_time),
					body.angular_velocity / angular_speed);
				body.rotation = glm::normalize(rotation_delta * body.rotation);
			}
		}
	}

	// Finds planet components once, then resolves sphere fallback contacts and mesh-surface
	// contacts against their scalar fields. Contact impulses update body motion.
	void World::resolvePlanetCollisions()
	{
		Watcher watcher("World::planet collisions");
		DynamicArray<PlanetCollisionTarget> planets;
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
				planets.push(PlanetCollisionTarget{
					body_id,
					static_cast<const PlanetComponent*>(slot.getData())});
			}
		}
		if(planets.size() == 0) return;

		for(WorldBodyId moving_body_id : data.getUsedWorldBodyIds())
		{
			if(!data.isWorldBodyActive(moving_body_id)) continue;
			WorldBody& moving_body = data.getWorldBody(moving_body_id);
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
			const BodyMassProperties mass_properties = calculateMassProperties(moving_body);
			if(mass_properties.mass <= 0.0) continue;
			bool has_surface_mesh = false;
			for(ChunkId chunk_id : moving_body.chunk_ids)
			{
				if(data.isChunkActive(chunk_id)
					&& data.getChunk(chunk_id).ground_triangle_ids.size() > 0)
				{
					has_surface_mesh = true;
					break;
				}
			}

			if(moving_body.collision_radius > 0.0 && !has_surface_mesh)
			{
				for(const PlanetCollisionTarget& target : planets)
				{
					if(target.body_id == moving_body_id || !data.isWorldBodyActive(target.body_id)) continue;
					const WorldBody& planet_body = data.getWorldBody(target.body_id);
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
				if(!data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = data.getChunk(chunk_id);
				for(const PlanetCollisionTarget& target : planets)
				{
					if(target.body_id == moving_body_id) continue;
					collideChunkSurfaceWithPlanet(*this, moving_body, chunk, mass_properties, target);
				}
				}
			}
		}
}
