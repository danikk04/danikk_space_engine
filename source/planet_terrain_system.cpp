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

		/** @brief Samples a low-frequency pair of noise sheets whose intersection forms rounded tunnels. */
		double sampleTunnelCaveDensity(const glm::dvec3& position, uint64 noise_seed, double width_bonus)
		{
			const glm::dvec3 noise_position = position * 0.012;
			const double first_sheet = math::fractalNoise3D(
				noise_position + planetNoiseSeedOffset(noise_seed ^ 0x2D358DCCAA6C78A5ull), 2, 0.5);
			const double second_sheet = math::fractalNoise3D(
				noise_position + planetNoiseSeedOffset(noise_seed ^ 0x8BB84B93962EACC9ull), 2, 0.5);
			const double distance_from_tunnel_axis = std::sqrt(
				first_sheet * first_sheet + second_sheet * second_sheet);
			constexpr double tunnel_radius = 0.06;
			constexpr double maximum_tunnel_radius_bonus = 0.06;
			return (distance_from_tunnel_axis - (tunnel_radius + maximum_tunnel_radius_bonus * width_bonus)) * 100.0;
		}

		/** @brief Samples frequent, smaller noise caves intended for progressively deeper terrain. */
		double sampleDeepCaveDensity(const glm::dvec3& position, uint64 noise_seed, double width_bonus)
		{
			const double cavern_noise = std::abs(math::fractalNoise3D(
				position * 0.009 + planetNoiseSeedOffset(noise_seed ^ 0xA511E9B3u), 4, 0.5));
			constexpr double cavern_threshold = 0.04;
			constexpr double maximum_cavern_threshold_bonus = 0.04;
			return (cavern_noise - (cavern_threshold + maximum_cavern_threshold_bonus * width_bonus)) * 100.0;
		}

		/** @brief Returns a smooth cave-width bonus that peaks at 200 m depth. */
		double sampleCaveWidthBonus(double depth)
		{
			return glm::smoothstep(100.0, 200.0, depth)
				* (1.0 - glm::smoothstep(200.0, 300.0, depth));
		}

		/** @brief Returns the planet component referenced by an active component slot. */
		const PlanetComponent& getPlanetComponent(const World& world, WorldBodyComponentSlotId component_id)
		{
			if(!world.world_body_data.isWorldBodyComponentSlotActive(component_id))
				danikk_engine::formatFatalError("Planet terrain query references inactive component slot %", component_id);

			const WorldBodyComponentSlot& slot = world.world_body_data.getWorldBodyComponentSlot(component_id);
			if(slot.type_id != WorldBodyComponentTypeId::planet)
				danikk_engine::formatFatalError("Planet terrain query references non-planet component slot %", component_id);
			return *static_cast<const PlanetComponent*>(slot.getData());
		}

		double samplePlanetDensity(const glm::dvec3& position, double radius, uint64 noise_seed,
			const DynamicArray<PlanetTerrainEdit>& edits, double core_radius)
		{
			const double distance_from_center = glm::length(position);
			double density = radius + samplePlanetTerrainHeight(position, noise_seed) - distance_from_center;
			const double depth = density;
			const double width_bonus = sampleCaveWidthBonus(depth);
			const double tunnel_density = sampleTunnelCaveDensity(position, noise_seed, width_bonus);
			const double deep_cave_density = sampleDeepCaveDensity(position, noise_seed, width_bonus);
			const double deep_cave_weight = glm::smoothstep(100.0, 300.0, depth);
			const double raw_cave_density = tunnel_density * (1.0 - deep_cave_weight)
				+ deep_cave_density * deep_cave_weight;
			const double cave_fade = 1.0 - glm::smoothstep(1000.0, 2500.0, depth);
			const double cave_density = glm::mix(1.0, raw_cave_density, cave_fade);
			density = std::min(density, cave_density);

			for(const PlanetTerrainEdit& edit : edits)
			{
				const double distance = glm::length(position - edit.center);
				if(edit.adds_solid_terrain) density = std::max(density, edit.radius - distance);
				else density = std::min(density, distance - edit.radius);
			}
			return std::max(density, samplePlanetCoreDensity(position, distance_from_center, noise_seed, core_radius));
		}

		MaterialId samplePlanetMaterial(const glm::dvec3& position, double radius, uint64 noise_seed)
		{
			constexpr double grass_layer_depth = 1.0;
			constexpr double dirt_layer_depth = 5.0;
			const double distance_from_center = glm::length(position);
			if(distance_from_center < 0.000001) return default_materials::stone;
			const glm::dvec3 direction = position / distance_from_center;
			const double surface_depth = samplePlanetSurfaceRadius(direction, radius, noise_seed) - distance_from_center;
			if(surface_depth <= grass_layer_depth) return default_materials::grass;
			if(surface_depth <= dirt_layer_depth) return default_materials::dirt;
			return default_materials::stone;
		}
	}

	/** @copydoc getPlanetDensity(const PlanetTerrainSnapshot&, const glm::dvec3&) */
	double PlanetTerrainSystem::getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return samplePlanetDensity(position, snapshot.planet_radius, snapshot.noise_seed,
			snapshot.terrain_edits, snapshot.core_radius);
	}

	/** @copydoc getPlanetMaterial(const PlanetTerrainSnapshot&, const glm::dvec3&) */
	MaterialId PlanetTerrainSystem::getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return samplePlanetMaterial(position, snapshot.planet_radius, snapshot.noise_seed);
	}

	/** @copydoc getPlanetDensity */
	double PlanetTerrainSystem::getPlanetDensity(WorldBodyComponentSlotId planet_component_id, const glm::dvec3& position) const
	{
		const PlanetComponent& planet = getPlanetComponent(this->world, planet_component_id);
		return samplePlanetDensity(position, planet.radius, planet.noise_seed,
			planet.terrain_edits, planet.core_radius);
	}

	/** @copydoc getPlanetMaterial(const World&, WorldBodyComponentSlotId, const glm::dvec3&) */
	MaterialId PlanetTerrainSystem::getPlanetMaterial(WorldBodyComponentSlotId planet_component_id, const glm::dvec3& position) const
	{
		const PlanetComponent& planet = getPlanetComponent(this->world, planet_component_id);
		return samplePlanetMaterial(position, planet.radius, planet.noise_seed);
	}

	/** @copydoc PlanetTerrainSystem::getPlanetHeightAboveSurface */
	double PlanetTerrainSystem::getPlanetHeightAboveSurface(
		WorldBodyComponentSlotId planet_component_id, const glm::dvec3& position) const
	{
		const PlanetComponent& planet = getPlanetComponent(this->world, planet_component_id);
		const double distance_from_center = glm::length(position);
		if(distance_from_center <= 1e-9) return -planet.radius;
		const glm::dvec3 direction = position / distance_from_center;
		return distance_from_center - samplePlanetSurfaceRadius(direction, planet.radius, planet.noise_seed);
	}

}
