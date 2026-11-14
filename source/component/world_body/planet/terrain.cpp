#include <cmath>
#include <cstdint>
#include <algorithm>

#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

namespace danikk_space_engine
{
	namespace
	{
		glm::dvec3 planetNoiseSeedOffset(uint64 seed)
		{
			const uint32 first = static_cast<uint32>(seed) * 0x9E3779B9u;
			const uint32 second = static_cast<uint32>(seed >> 32) * 0x85EBCA6Bu;
			const uint32 third = (first ^ second) * 0xC2B2AE35u;
			return glm::dvec3(first % 10000u, second % 10000u, third % 10000u) * 0.01;
		}

		double samplePlanetTerrainHeight(const glm::dvec3& position, uint64 noise_seed)
		{
			return math::fractalNoise3D(position * 0.02 + planetNoiseSeedOffset(noise_seed), 3, 0.5) * 16.0;
		}
	}

	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits)
	{
		const double distance_from_center = glm::length(position_relative_to_planet_center);
		double density = radius
			+ samplePlanetTerrainHeight(position_relative_to_planet_center, noise_seed)
			- distance_from_center;
		const double cave_noise = std::abs(math::fractalNoise3D(
			position_relative_to_planet_center * 0.009 + planetNoiseSeedOffset(noise_seed ^ 0xA511E9B3u),
			4,
			0.5));
		const double cave_density = (cave_noise - 0.07) * 100.0;
		density = std::min(density, cave_density);

		for(const PlanetTerrainEdit& edit : edits)
		{
			const double distance = glm::length(position_relative_to_planet_center - edit.center);
			if(edit.add_material) density = std::max(density, edit.radius - distance);
			else density = std::min(density, distance - edit.radius);
		}
		return density;
	}

}
