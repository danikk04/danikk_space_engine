#include <cmath>
#include <cstdint>
#include <algorithm>

#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Converts a planet seed into a deterministic three-dimensional noise offset.
		 */
		glm::dvec3 planetNoiseSeedOffset(uint64 seed)
		{
			const uint32 first = static_cast<uint32>(seed) * 0x9E3779B9u;
			const uint32 second = static_cast<uint32>(seed >> 32) * 0x85EBCA6Bu;
			const uint32 third = (first ^ second) * 0xC2B2AE35u;
			return glm::dvec3(first % 10000u, second % 10000u, third % 10000u) * 0.01;
		}

		/**
		 * @brief Evaluates three-octave seeded fractal noise as radial terrain-height displacement.
		 * @param position Planet-centered sample position used as the noise coordinate.
		 * @param noise_seed Seed that offsets the noise domain deterministically.
		 * @return Height displacement in world units, scaled to the current terrain amplitude.
		 */
		double samplePlanetTerrainHeight(const glm::dvec3& position, uint64 noise_seed)
		{
			return math::fractalNoise3D(position * 0.02 + planetNoiseSeedOffset(noise_seed), 3, 0.5) * 16.0;
		}

		/**
		 * @brief Iteratively estimates the noisy surface radius along a radial direction.
		 * @param direction Normalized direction from the planet center.
		 * @param radius Preset mean surface radius.
		 * @param noise_seed Seed used by the terrain-height field.
		 * @return Mean radius plus the sampled height displacement at the surface location.
		 */
		double samplePlanetSurfaceRadius(const glm::dvec3& direction, double radius, uint64 noise_seed)
		{
			double surface_radius = radius;
			for(uint8 iteration = 0; iteration < 5; iteration++)
			{
				surface_radius = radius + samplePlanetTerrainHeight(direction * surface_radius, noise_seed);
			}
			return surface_radius;
		}

		/**
		 * @brief Returns signed density for the slightly noisy solid-core boundary.
		 * @param position Planet-centered sample coordinate.
		 * @param distance_from_center Distance of the sample from the planet origin.
		 * @param noise_seed Seed used for the independently offset core noise.
		 * @param core_radius Preset mean core radius.
		 * @return Positive density inside the noisy core and negative density outside it.
		 */
		double samplePlanetCoreDensity(
			const glm::dvec3& position,
			double distance_from_center,
			uint64 noise_seed,
			double core_radius)
		{
			constexpr double core_surface_roughness = 0.75;
			const double surface_noise = math::fractalNoise3D(
				position * 0.25 + planetNoiseSeedOffset(noise_seed ^ 0x6C8E9CF570932BD5ull),
				3,
				0.5);
			const double uneven_radius = core_radius + core_surface_roughness * (surface_noise + 1.0);
			return uneven_radius - distance_from_center;
		}
	}

	/**
	 * @brief Evaluates the signed field by combining a noisy shell, cave mask, edit stamps, and core.
	 * @param position_relative_to_planet_center Sample coordinate relative to planet origin.
	 * @param radius Mean outer surface radius.
	 * @param noise_seed Seed controlling terrain, cave, and core noise domains.
	 * @param edits Persistent additive and subtractive spherical terrain edits.
	 * @param core_radius Mean radius of the core that remains solid despite cave carving.
	 * @return Signed density: positive is solid, negative is air, and zero is an isosurface.
	 * The radial shell is `radius + height_noise - distance`. A four-octave cave
	 * field with frequency 0.009 and threshold 0.04 carves the shell via min().
	 * Add stamps union spheres with max(); remove stamps subtract spheres with
	 * min(). The seeded, mildly rough core is finally unioned with max(), keeping
	 * the interior solid regardless of cave or edit values.
	 */
	double samplePlanetScalarField(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		double core_radius)
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
			position_relative_to_planet_center, distance_from_center, noise_seed, core_radius));
		return density;
	}

	/**
	 * @brief Assigns grass to samples within three metres of the noisy outer radius and stone deeper inside.
	 * @param position_relative_to_planet_center Sample point relative to planet origin.
	 * @param radius Mean planet surface radius.
	 * @param noise_seed Seed used to estimate the local noisy surface radius.
	 * @return Grass when `surface_radius - distance <= 3 m`, otherwise stone; the
	 *         exact center is stone. This one-sided threshold also classifies points
	 *         outside the nominal surface as grass.
	 */
	MaterialId getPlanetSurfaceMaterial(
		const glm::dvec3& position_relative_to_planet_center,
		double radius,
		uint64 noise_seed)
	{
		constexpr double grass_layer_depth = 3.0;
		const double distance_from_center = glm::length(position_relative_to_planet_center);
		if(distance_from_center < 0.000001) return default_materials::stone;

		const glm::dvec3 direction = position_relative_to_planet_center / distance_from_center;
		const double surface_depth = samplePlanetSurfaceRadius(direction, radius, noise_seed) - distance_from_center;
		return surface_depth <= grass_layer_depth
			? default_materials::grass
			: default_materials::stone;
	}

}
