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

		struct CellBounds
		{
			glm::dvec3 local_center = glm::dvec3(0.0);
			double radius = 0.0;
		};

		struct BodyMassProperties
		{
			double mass = 0.0;
			glm::dvec3 local_center_of_mass = glm::dvec3(0.0);
			double scalar_inertia = 0.0;
		};

		bool getCellBounds(const WorldData& data, const Chunk& chunk, const VolumeCell& cell, CellBounds& bounds)
		{
			const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
			if(topology.point_count < 4) return false;

			vec3 center(0.0f);
			for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
			{
				const PointId point_id = cell.points[point_index];
				if(!data.isPointActive(point_id)) return false;
				center += data.getPoint(point_id).position;
			}
			center /= static_cast<float>(topology.point_count);

			float radius = 0.0f;
			for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
			{
				const vec3 delta = data.getPoint(cell.points[point_index]).position - center;
				radius = glm::max(radius, glm::length(delta));
			}
			bounds.local_center = chunk.position + glm::dvec3(center);
			bounds.radius = static_cast<double>(radius);
			return true;
		}

		BodyMassProperties calculateMassProperties(const WorldData& data, const WorldBody& body)
		{
			BodyMassProperties properties;
			for(ChunkId chunk_id : body.chunk_ids)
			{
				if(!data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = data.getChunk(chunk_id);
				for(VolumeCellId cell_id : chunk.volume_cell_ids)
				{
					if(!data.isVolumeCellActive(cell_id)) continue;
					const VolumeCell& cell = data.getVolumeCell(cell_id);
					CellBounds bounds;
					if(!getCellBounds(data, chunk, cell, bounds)) continue;
					const double mass = cell.mass > 0.0f ? cell.mass : 1.0;
					properties.local_center_of_mass += bounds.local_center * mass;
					properties.mass += mass;
				}
			}
			if(properties.mass <= 0.0) return properties;
			properties.local_center_of_mass /= properties.mass;

			for(ChunkId chunk_id : body.chunk_ids)
			{
				if(!data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = data.getChunk(chunk_id);
				for(VolumeCellId cell_id : chunk.volume_cell_ids)
				{
					if(!data.isVolumeCellActive(cell_id)) continue;
					const VolumeCell& cell = data.getVolumeCell(cell_id);
					CellBounds bounds;
					if(!getCellBounds(data, chunk, cell, bounds)) continue;
					const double mass = cell.mass > 0.0f ? cell.mass : 1.0;
					const glm::dvec3 offset = bounds.local_center - properties.local_center_of_mass;
					properties.scalar_inertia += 0.4 * mass * bounds.radius * bounds.radius
						+ mass * glm::dot(offset, offset);
				}
			}
			properties.scalar_inertia = glm::max(properties.scalar_inertia, 0.000001);
			return properties;
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

	// Finds planet components once, then resolves contacts between each VolumeCell bounding sphere
	// and the planet scalar field. A support point approximates the contact location; normal and
	// friction impulses update linear/angular velocity while positional correction prevents sinking.
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
			const BodyMassProperties mass_properties = calculateMassProperties(data, moving_body);
			if(mass_properties.mass <= 0.0) continue;

			for(ChunkId chunk_id : moving_body.chunk_ids)
			{
				if(!data.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = data.getChunk(chunk_id);
				for(VolumeCellId cell_id : chunk.volume_cell_ids)
				{
					if(!data.isVolumeCellActive(cell_id)) continue;
					const VolumeCell& cell = data.getVolumeCell(cell_id);
					const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
					CellBounds bounds;
					if(!getCellBounds(data, chunk, cell, bounds)) continue;

					for(const PlanetCollisionTarget& target : planets)
					{
						if(target.body_id == moving_body_id) continue;
						if(!data.isWorldBodyActive(target.body_id)) continue;
						const WorldBody& planet_body = data.getWorldBody(target.body_id);
						const glm::dquat inverse_planet_rotation = glm::conjugate(planet_body.rotation);
						const glm::dvec3 cell_world_center = moving_body.position
							+ moving_body.rotation * bounds.local_center;
						const glm::dvec3 field_position = inverse_planet_rotation
							* (cell_world_center - planet_body.position);
						const double field_radius = target.planet->radius;
						const auto sample_field = [&](const glm::dvec3& position)
						{
							return samplePlanetScalarField(
								position,
								field_radius,
								target.planet->noise_seed,
								target.planet->terrain_edits);
						};

						const double density = sample_field(field_position);
						const double penetration = density + bounds.radius;
						if(penetration <= 0.0) continue;

						const double gradient_step = planet_voxel_size * 0.5;
						glm::dvec3 gradient(0.0);
						for(uint8 axis = 0; axis < 3; axis++)
						{
							glm::dvec3 offset(0.0);
							offset[axis] = gradient_step;
							gradient[axis] = (sample_field(field_position + offset)
								- sample_field(field_position - offset)) / (2.0 * gradient_step);
						}

						glm::dvec3 outward_direction = -gradient;
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
						const glm::dvec3 world_outward_direction = planet_body.rotation * outward_direction;

						const glm::dvec3 local_outward_direction = glm::conjugate(moving_body.rotation)
							* world_outward_direction;
						double minimum_support_projection = std::numeric_limits<double>::infinity();
						for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
						{
							const glm::dvec3 point_position = chunk.position
								+ glm::dvec3(data.getPoint(cell.points[point_index]).position);
							minimum_support_projection = glm::min(minimum_support_projection,
								glm::dot(point_position - bounds.local_center, local_outward_direction));
						}
						glm::dvec3 support_point_local(0.0);
						uint8 support_point_count = 0;
						for(uint8 point_index = 0; point_index < topology.point_count; point_index++)
						{
							const glm::dvec3 point_position = chunk.position
								+ glm::dvec3(data.getPoint(cell.points[point_index]).position);
							const double projection = glm::dot(
								point_position - bounds.local_center,
								local_outward_direction);
							if(projection > minimum_support_projection + 0.0001) continue;
							support_point_local += point_position;
							support_point_count++;
						}
						if(support_point_count > 0)
						{
							support_point_local /= static_cast<double>(support_point_count);
						}
						else
						{
							support_point_local = bounds.local_center
								- local_outward_direction * bounds.radius;
						}

						moving_body.position += world_outward_direction * (penetration + 0.001);
						const glm::dvec3 world_center_of_mass = moving_body.position
							+ moving_body.rotation * mass_properties.local_center_of_mass;
						const glm::dvec3 contact_point = moving_body.position
							+ moving_body.rotation * support_point_local;
						const glm::dvec3 contact_offset = contact_point - world_center_of_mass;
						const double inverse_mass = 1.0 / mass_properties.mass;
						const double inverse_inertia = 1.0 / mass_properties.scalar_inertia;
						const glm::dvec3 normal_velocity = moving_body.linear_velocity
							+ glm::cross(moving_body.angular_velocity, contact_offset);
						const double inward_speed = glm::dot(normal_velocity, world_outward_direction);
						if(inward_speed < 0.0)
						{
							const glm::dvec3 normal_arm = glm::cross(contact_offset, world_outward_direction);
							const double normal_effective_mass = inverse_mass
								+ glm::dot(normal_arm, normal_arm) * inverse_inertia;
							const double normal_impulse_magnitude = -inward_speed / normal_effective_mass;
							applyImpulse(moving_body, mass_properties, contact_offset,
								world_outward_direction * normal_impulse_magnitude);

							const glm::dvec3 post_normal_velocity = moving_body.linear_velocity
								+ glm::cross(moving_body.angular_velocity, contact_offset);
							const glm::dvec3 tangent_velocity = post_normal_velocity
								- world_outward_direction * glm::dot(post_normal_velocity, world_outward_direction);
							const double tangent_speed = glm::length(tangent_velocity);
								if(tangent_speed > 0.000001)
							{
								const glm::dvec3 tangent_direction = tangent_velocity / tangent_speed;
								const glm::dvec3 tangent_arm = glm::cross(contact_offset, tangent_direction);
								const double tangent_effective_mass = inverse_mass
									+ glm::dot(tangent_arm, tangent_arm) * inverse_inertia;
								const double friction_coefficient = 0.6;
								const double friction_impulse_magnitude = glm::clamp(
									-tangent_speed / tangent_effective_mass,
									-friction_coefficient * normal_impulse_magnitude,
									friction_coefficient * normal_impulse_magnitude);
								applyImpulse(moving_body, mass_properties, contact_offset,
									tangent_direction * friction_impulse_magnitude);
								}
							}
						}
					}
				}
			}
		}
}
