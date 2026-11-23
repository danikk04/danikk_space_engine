#include <danikk_space_engine/systems/environment/environment.h>
#include <danikk_framework/log.h>

#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	void EnvironmentSystem::init() { danikk_framework::formatLogInfo("Init EnvironmentSystem"); }
	void EnvironmentSystem::shutdown() { danikk_framework::formatLogInfo("Shutdown EnvironmentSystem"); }
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

		for(const PlanetData& planet : world.getPlanetSystem().getPlanets())
		{
			const PlanetSettings& settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
			if(settings.surface_gravity <= 0.0 || settings.radius <= 0.0) continue;

				const glm::dvec3 toward_planet = planet.position - world_position;
				const double distance = glm::length(toward_planet);
				if(distance <= 0.000001) continue;
				const double distance_ratio = settings.radius / distance;
				const double magnitude = distance > settings.radius
					? settings.surface_gravity * distance_ratio * distance_ratio
					: settings.surface_gravity * distance_ratio;
			gravity += toward_planet / distance * magnitude;
		}
		return gravity;
	}
}
