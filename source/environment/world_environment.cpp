#include <danikk_space_engine/environment.h>

#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	/**
	 * @brief Returns unavailable until a temperature provider is registered.
	 * @param world_position Unused sample position.
	 * @param world_time_seconds Unused simulation time.
	 * @return Default sample with available false.
	 */
	TemperatureSample EnvironmentSystem::sampleTemperatureAt(
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
	RadiationSample EnvironmentSystem::sampleRadiationAt(
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
	glm::dvec3 EnvironmentSystem::sampleGravityAt(const glm::dvec3& world_position) const
	{
		glm::dvec3 gravity(0.0);

		for(const PlanetData& planet : world.planets.getPlanets())
		{
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
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
		return gravity;
	}
}
