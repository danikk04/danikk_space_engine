#include <danikk_space_engine/world_environment.h>

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	// No temperature providers are registered yet. Availability distinguishes a missing field from
	// a valid physical temperature of zero kelvin.
	TemperatureSample WorldEnvironment::sampleTemperatureAt(
		const World& world,
		const glm::dvec3& world_position,
		double world_time_seconds) const
	{
		(void)world;
		(void)world_position;
		(void)world_time_seconds;
		return TemperatureSample{};
	}

	// No radiation providers are registered yet. Availability distinguishes a missing field from
	// a valid zero dose rate.
	RadiationSample WorldEnvironment::sampleRadiationAt(
		const World& world,
		const glm::dvec3& world_position,
		double world_time_seconds) const
	{
		(void)world;
		(void)world_position;
		(void)world_time_seconds;
		return RadiationSample{};
	}

	glm::dvec3 WorldEnvironment::sampleGravityAt(const World& world, const glm::dvec3& world_position) const
	{
		glm::dvec3 gravity(0.0);
		for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world.data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!world.data.isWorldBodyComponentSlotActive(slot_id)) continue;
				const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(slot_id);
				if(slot.type_id != WorldBodyComponentTypeId::planet) continue;
				const PlanetComponent& planet = *static_cast<const PlanetComponent*>(slot.getData());
				if(planet.surface_gravity <= 0.0 || planet.radius <= 0.0) continue;

				const glm::dvec3 toward_planet = body.position - world_position;
				const double distance = glm::length(toward_planet);
				if(distance <= 0.000001) continue;
				const double distance_ratio = planet.radius / distance;
				const double magnitude = distance > planet.radius
					? planet.surface_gravity * distance_ratio * distance_ratio
					: planet.surface_gravity * distance_ratio;
				gravity += toward_planet / distance * magnitude;
			}
		}
		return gravity;
	}
}
