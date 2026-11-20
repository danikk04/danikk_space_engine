#include <algorithm>
#include <cmath>

#include <danikk_space_engine/config.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

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

		struct PlanetTerrainNoiseLayer
		{
			double frequency;
			double amplitude;
			uint64 seed_offset;
		};

		constexpr PlanetTerrainNoiseLayer planet_terrain_noise_layers[] = {
			{1.0 / 10000000.0, 6000.0, 0x243F6A8885A308D3ull},
			{1.0 / 1250000.0, 2750.0, 0x13198A2E03707344ull},
			{1.0 / 125000.0, 1000.0, 0xA4093822299F31D0ull},
			{1.0 / 12500.0, 300.0, 0x082EFA98EC4E6C89ull},
			{1.0 / 1250.0, 60.0, 0x452821E638D01377ull},
			{1.0 / 125.0, 8.5, 0xBE5466CF34E90C6Cull},
			{1.0 / 30.0, 3.0, 0x3F84D5B5B5470917ull}};

		bool includesCaves(PlanetTerrainMode mode)
		{
			return mode == PlanetTerrainMode::cave_field
				|| mode == PlanetTerrainMode::cave_field_with_edits;
		}

		bool includesEdits(PlanetTerrainMode mode)
		{
			return mode == PlanetTerrainMode::heightmap_with_edits
				|| mode == PlanetTerrainMode::cave_field_with_edits;
		}

		double samplePlanetTerrainHeight(const glm::dvec3& position, uint64 noise_seed,
			double minimum_wavelength = 0.0)
		{
			double height = 0.0;
			for(const PlanetTerrainNoiseLayer& layer : planet_terrain_noise_layers)
			{
				if(minimum_wavelength > 0.0 && 1.0 / layer.frequency < minimum_wavelength) continue;
				const glm::dvec3 noise_position = position * layer.frequency
					+ planetNoiseSeedOffset(noise_seed ^ layer.seed_offset);
				height += math::simplexNoise3D(noise_position) * layer.amplitude;
			}
			return height;
		}

		double samplePlanetSurfaceRadius(const glm::dvec3& direction, double radius, uint64 noise_seed,
			double minimum_wavelength = 0.0)
		{
			double surface_radius = radius;
			for(uint8 iteration = 0; iteration < 2; iteration++)
				surface_radius = radius + samplePlanetTerrainHeight(
					direction * surface_radius, noise_seed, minimum_wavelength);
			return surface_radius;
		}

		double samplePlanetSurfaceRadius(const glm::dvec3& direction, double radius, uint64 noise_seed,
			const PlanetTerrainEditPool& edit_pool, const DynamicArray<PlanetTerrainEditId>& edit_ids,
			double minimum_wavelength, PlanetTerrainMode mode)
		{
			double surface_radius = samplePlanetSurfaceRadius(
				direction, radius, noise_seed, minimum_wavelength);
			if(includesEdits(mode))
				for(PlanetTerrainEditId edit_id : edit_ids)
					surface_radius = edit_pool[edit_id].applyToSurfaceRadius(direction, surface_radius);
			return surface_radius;
		}

		double samplePlanetCoreDensity(const glm::dvec3& position, double distance_from_center,
			uint64 noise_seed, double core_radius)
		{
			constexpr double core_surface_roughness = 0.75;
			const double surface_noise = math::fractalNoise3D(
				position * 0.25 + planetNoiseSeedOffset(noise_seed ^ 0x6C8E9CF570932BD5ull), 3, 0.5);
			return core_radius + core_surface_roughness * (surface_noise + 1.0) - distance_from_center;
		}

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

		double sampleDeepCaveDensity(const glm::dvec3& position, uint64 noise_seed, double width_bonus)
		{
			const double cavern_noise = std::abs(math::fractalNoise3D(
				position * 0.009 + planetNoiseSeedOffset(noise_seed ^ 0xA511E9B3u), 4, 0.5));
			constexpr double cavern_threshold = 0.04;
			constexpr double maximum_cavern_threshold_bonus = 0.04;
			return (cavern_noise - (cavern_threshold + maximum_cavern_threshold_bonus * width_bonus)) * 100.0;
		}

		double sampleCaveWidthBonus(double depth)
		{
			return glm::smoothstep(100.0, 200.0, depth)
				* (1.0 - glm::smoothstep(200.0, 300.0, depth));
		}

		double samplePlanetDensity(const glm::dvec3& position, double radius, uint64 noise_seed,
			const PlanetTerrainEditPool& edit_pool, const DynamicArray<PlanetTerrainEditId>& edit_ids,
			double core_radius, PlanetTerrainMode mode)
		{
			const double distance_from_center = glm::length(position);
			double density = radius + samplePlanetTerrainHeight(position, noise_seed) - distance_from_center;
			if(includesCaves(mode))
			{
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
			}

			if(includesEdits(mode))
				for(PlanetTerrainEditId edit_id : edit_ids)
					density = edit_pool[edit_id].applyToScalarField(position, density);
			return std::max(density,
				samplePlanetCoreDensity(position, distance_from_center, noise_seed, core_radius));
		}

		MaterialId samplePlanetMaterial(const glm::dvec3& position, double radius, uint64 noise_seed,
			const PlanetTerrainEditPool& edit_pool, const DynamicArray<PlanetTerrainEditId>& edit_ids,
			PlanetTerrainMode mode)
		{
			constexpr double grass_layer_depth = 1.0;
			constexpr double dirt_layer_depth = 5.0;
			const double distance_from_center = glm::length(position);
			MaterialId material_id = default_materials::stone;
			if(distance_from_center >= 0.000001)
			{
				const glm::dvec3 direction = position / distance_from_center;
				const double surface_depth = samplePlanetSurfaceRadius(direction, radius, noise_seed) - distance_from_center;
				if(surface_depth <= grass_layer_depth) material_id = default_materials::grass;
				else if(surface_depth <= dirt_layer_depth) material_id = default_materials::dirt;
			}
			if(includesEdits(mode))
				for(PlanetTerrainEditId edit_id : edit_ids)
					material_id = edit_pool[edit_id].applyToMaterial(position, material_id);
			return material_id;
		}
	}

	double PlanetSystem::getPlanetDensity(
		WorldBodyId world_body_id, const glm::dvec3& position, PlanetTerrainMode mode) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
		return samplePlanetDensity(position, settings.radius, settings.noise_seed,
			*planet.terrain_edit_pool, planet.terrain_edit_ids, settings.core_radius, mode);
	}

	MaterialId PlanetSystem::getPlanetMaterial(
		WorldBodyId world_body_id, const glm::dvec3& position, PlanetTerrainMode mode) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
		return samplePlanetMaterial(position, settings.radius, settings.noise_seed,
			*planet.terrain_edit_pool, planet.terrain_edit_ids, mode);
	}

	double PlanetSystem::getPlanetSurfaceRadius(WorldBodyId world_body_id,
		const glm::dvec3& direction, PlanetTerrainMode mode, double minimum_wavelength) const
	{
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12) return 0.0;
		const PlanetData& planet = getPlanet(world_body_id);
		const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
		return samplePlanetSurfaceRadius(direction / direction_length, settings.radius, settings.noise_seed,
			*planet.terrain_edit_pool, planet.terrain_edit_ids, minimum_wavelength, mode);
	}

	double PlanetSystem::getPlanetHeightAboveSurface(WorldBodyId world_body_id, const glm::dvec3& position) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
		const double distance_from_center = glm::length(position);
		if(distance_from_center <= 1e-9) return -settings.radius;
		const glm::dvec3 direction = position / distance_from_center;
		return distance_from_center - samplePlanetSurfaceRadius(
			direction, settings.radius, settings.noise_seed,
			*planet.terrain_edit_pool, planet.terrain_edit_ids, 0.0,
			PlanetTerrainMode::heightmap_with_edits);
	}
}
