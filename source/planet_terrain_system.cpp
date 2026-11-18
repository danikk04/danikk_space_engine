#include <algorithm>
#include <cmath>

#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <danikk_space_engine/planet_terrain_system.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Converts a planet seed into a deterministic three-dimensional noise offset. */
		glm::dvec3 planetNoiseSeedOffset(uint64 seed)
		{
			const uint32 first = static_cast<uint32>(seed) * 0x9E3779B9u;
			const uint32 second = static_cast<uint32>(seed >> 32) * 0x85EBCA6Bu;
			const uint32 third = (first ^ second) * 0xC2B2AE35u;
			return glm::dvec3(first % 10000u, second % 10000u, third % 10000u) * 0.01;
		}

		/** @brief Samples the seeded radial displacement of the planet surface. */
		double samplePlanetTerrainHeight(const glm::dvec3& position, uint64 noise_seed)
		{
			return math::fractalNoise3D(position * 0.02 + planetNoiseSeedOffset(noise_seed), 3, 0.5) * 16.0;
		}

		/** @brief Estimates the noisy surface radius in the given radial direction. */
		double samplePlanetSurfaceRadius(const glm::dvec3& direction, double radius, uint64 noise_seed)
		{
			double surface_radius = radius;
			// Two fixed-point steps are sufficient for the low-amplitude radial noise and
			// avoid repeating five multi-octave samples for every rendered triangle.
			for(uint8 iteration = 0; iteration < 2; iteration++)
				surface_radius = radius + samplePlanetTerrainHeight(direction * surface_radius, noise_seed);
			return surface_radius;
		}

		/** @brief Keeps the planet core solid with a mildly noisy boundary. */
		double samplePlanetCoreDensity(const glm::dvec3& position, double distance_from_center,
			uint64 noise_seed, double core_radius)
		{
			constexpr double core_surface_roughness = 0.75;
			const double surface_noise = math::fractalNoise3D(
				position * 0.25 + planetNoiseSeedOffset(noise_seed ^ 0x6C8E9CF570932BD5ull), 3, 0.5);
			return core_radius + core_surface_roughness * (surface_noise + 1.0) - distance_from_center;
		}

		/** @brief Returns the planet component referenced by an active component slot. */
		const PlanetComponent& getPlanetComponent(const World& world, WorldBodyComponentSlotId component_id)
		{
			if(!world.data.isWorldBodyComponentSlotActive(component_id))
				danikk_engine::formatFatalError("Planet terrain query references inactive component slot %", component_id);

			const WorldBodyComponentSlot& slot = world.data.getWorldBodyComponentSlot(component_id);
			if(slot.type_id != WorldBodyComponentTypeId::planet)
				danikk_engine::formatFatalError("Planet terrain query references non-planet component slot %", component_id);
			return *static_cast<const PlanetComponent*>(slot.getData());
		}

		double getPlanetDensity(const glm::dvec3& position, double radius, uint64 noise_seed,
			const DynamicArray<PlanetTerrainEdit>& edits, double core_radius)
		{
			const double distance_from_center = glm::length(position);
			double density = radius + samplePlanetTerrainHeight(position, noise_seed) - distance_from_center;
			const double cave_noise = std::abs(math::fractalNoise3D(
				position * 0.009 + planetNoiseSeedOffset(noise_seed ^ 0xA511E9B3u), 4, 0.5));
			constexpr double cave_threshold = 0.04;
			density = std::min(density, (cave_noise - cave_threshold) * 100.0);

			for(const PlanetTerrainEdit& edit : edits)
			{
				const double distance = glm::length(position - edit.center);
				if(edit.adds_solid_terrain) density = std::max(density, edit.radius - distance);
				else density = std::min(density, distance - edit.radius);
			}
			return std::max(density, samplePlanetCoreDensity(position, distance_from_center, noise_seed, core_radius));
		}

		MaterialId getPlanetMaterial(const glm::dvec3& position, double radius, uint64 noise_seed)
		{
			constexpr double grass_layer_depth = 3.0;
			const double distance_from_center = glm::length(position);
			if(distance_from_center < 0.000001) return default_materials::stone;
			const glm::dvec3 direction = position / distance_from_center;
			const double surface_depth = samplePlanetSurfaceRadius(direction, radius, noise_seed) - distance_from_center;
			return surface_depth <= grass_layer_depth ? default_materials::grass : default_materials::stone;
		}
	}

	/** @copydoc getPlanetDensity(const PlanetTerrainSnapshot&, const glm::dvec3&) */
	double getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return getPlanetDensity(position, snapshot.planet_radius, snapshot.noise_seed,
			snapshot.terrain_edits, snapshot.core_radius);
	}

	/** @copydoc getPlanetMaterial(const PlanetTerrainSnapshot&, const glm::dvec3&) */
	MaterialId getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return getPlanetMaterial(position, snapshot.planet_radius, snapshot.noise_seed);
	}

	/** @copydoc getPlanetDensity */
	double getPlanetDensity(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& position)
	{
		const PlanetComponent& planet = getPlanetComponent(world, planet_component_id);
		return getPlanetDensity(position, planet.radius, planet.noise_seed,
			planet.terrain_edits, planet.core_radius);
	}

	/** @copydoc getPlanetMaterial(const World&, WorldBodyComponentSlotId, const glm::dvec3&) */
	MaterialId getPlanetMaterial(const World& world, WorldBodyComponentSlotId planet_component_id,
		const glm::dvec3& position)
	{
		const PlanetComponent& planet = getPlanetComponent(world, planet_component_id);
		return getPlanetMaterial(position, planet.radius, planet.noise_seed);
	}

}
