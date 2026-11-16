#pragma once

#include <glm/glm.hpp>

namespace danikk_space_engine
{
	class World;

	struct TemperatureSample
	{
		double temperature_kelvin = 0.0;
		bool available = false;
	};

	struct RadiationSample
	{
		double radiation_dose_rate_sv_per_second = 0.0;
		bool available = false;
	};

	class WorldEnvironment
	{
	public:
		TemperatureSample sampleTemperatureAt(
			const World& world,
			const glm::dvec3& world_position,
			double world_time_seconds) const;
		RadiationSample sampleRadiationAt(
			const World& world,
			const glm::dvec3& world_position,
			double world_time_seconds) const;
		glm::dvec3 sampleGravityAt(const World& world, const glm::dvec3& world_position) const;
	};
}
