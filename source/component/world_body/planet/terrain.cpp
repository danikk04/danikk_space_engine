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

		double samplePlanetCoreDensity(
			const glm::dvec3& position,
			double distance_from_center,
			uint64 noise_seed)
		{
			constexpr double core_radius = 5.0;
			constexpr double core_surface_roughness = 0.75;
			const double surface_noise = math::fractalNoise3D(
				position * 0.25 + planetNoiseSeedOffset(noise_seed ^ 0x6C8E9CF570932BD5ull),
				3,
				0.5);
			const double uneven_radius = core_radius + core_surface_roughness * (surface_noise + 1.0);
			return uneven_radius - distance_from_center;
		}
	}

	// Combines the radial terrain field, cave carving, persistent edits, and a deterministic noisy
	// solid core. The core floor is applied last, ensuring edits and cave noise cannot hollow out its
	// 5-metre interior while its outer boundary varies smoothly with seeded noise.
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
		constexpr double cave_threshold = 0.04;
		const double cave_density = (cave_noise - cave_threshold) * 100.0;
		density = std::min(density, cave_density);

		for(const PlanetTerrainEdit& edit : edits)
		{
			const double distance = glm::length(position_relative_to_planet_center - edit.center);
			if(edit.adds_solid_terrain) density = std::max(density, edit.radius - distance);
			else density = std::min(density, distance - edit.radius);
		}
		density = std::max(density, samplePlanetCoreDensity(
			position_relative_to_planet_center, distance_from_center, noise_seed));
		return density;
	}

}
