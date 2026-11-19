#include <danikk_space_engine/environment.h>

#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Returns unavailable until a temperature provider is registered.
	 * @param world_position Unused sample position.
	 * @param world_time_seconds Unused simulation time.
	 * @return Default sample with available false.
	 */
	TemperatureSample WorldEnvironment::sampleTemperatureAt(
		const glm::dvec3& world_position,
		double world_time_seconds) const
	{
		(void)world_position;
		(void)world_time_seconds;
		return TemperatureSample{};
	}

	/**
	 * @brief Returns unavailable until a radiation provider is registered.
	 * @param world_position Unused sample position.
	 * @param world_time_seconds Unused simulation time.
	 * @return Default sample with available false.
	 */
	RadiationSample WorldEnvironment::sampleRadiationAt(
		const glm::dvec3& world_position,
		double world_time_seconds) const
	{
		(void)world_position;
		(void)world_time_seconds;
		return RadiationSample{};
	}

	/**
	 * @brief Adds the radial gravity contribution of every active planet component.
	 * @param world_position Global position in double precision.
	 * @return Summed acceleration vector in m/s². Outside a planet the configured
	 *         surface magnitude follows inverse-square falloff; inside, magnitude is
	 *         surface_gravity * radius / distance and therefore increases toward the
	 *         center. Samples within 1e-6 units of a center are skipped.
	 */
	glm::dvec3 WorldEnvironment::sampleGravityAt(const glm::dvec3& world_position) const
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
