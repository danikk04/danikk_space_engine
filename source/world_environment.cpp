#include <danikk_space_engine/world_environment.h>

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
}
