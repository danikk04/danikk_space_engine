#include <algorithm>
#include <cmath>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/planet_system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/planet_system/terrain_edit_save.h>

namespace danikk_space_engine
{
	namespace
	{
		const PlanetSettings planet_settings[planet_settings_ids::count] =
		{
			{"Earth", default_planet_radius, default_planet_core_radius,
				planet_chunk_load_radius, 9.8, 0}
		};

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

		double samplePlanetSurfaceRadius(const glm::dvec3& direction, double radius, uint64 noise_seed)
		{
			double surface_radius = radius;
			for(uint8 iteration = 0; iteration < 2; iteration++)
				surface_radius = radius + samplePlanetTerrainHeight(direction * surface_radius, noise_seed);
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
			return std::max(density,
				samplePlanetCoreDensity(position, distance_from_center, noise_seed, core_radius));
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

	const PlanetSettings& getPlanetSettings(PlanetSettingsId id)
	{
		if(id >= planet_settings_ids::count)
			danikk_engine::formatFatalError("Invalid planet settings ID %", id);
		return planet_settings[id];
	}

	PlanetData& PlanetSystem::createPlanet(WorldBodyId world_body_id, const PlanetSettings& settings)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Cannot register a planet for inactive WorldBody %", world_body_id);
		if(PlanetData* existing = findPlanet(world_body_id)) return *existing;
		PlanetData planet;
		planet.world_body_id = world_body_id;
		planet.generation_id = next_generation_id++;
		if(next_generation_id == 0) next_generation_id = 1;
		planet.name = settings.name;
		planet.radius = settings.radius;
		planet.core_radius = settings.core_radius;
		planet.chunk_load_radius = settings.chunk_load_radius;
		planet.surface_gravity = settings.surface_gravity;
		planet.noise_seed = settings.noise_seed;
		planets.pushMove(planet);
		return planets[planets.size() - 1];
	}

	void PlanetSystem::removeQueuedRequests(const PlanetData& planet)
	{
		world.chunk_mesh_generation.cancelPlanetRequests(planet.world_body_id, planet.generation_id);
	}

	void PlanetSystem::destroyPlanet(WorldBodyId world_body_id)
	{
		for(size_t index = 0; index < planets.size(); ++index)
		{
			if(planets[index].world_body_id != world_body_id) continue;
			removeQueuedRequests(planets[index]);
			for(size_t shift = index + 1; shift < planets.size(); ++shift)
				planets[shift - 1] = static_cast<PlanetData&&>(planets[shift]);
			planets.resize(planets.size() - 1);
			return;
		}
	}

	bool PlanetSystem::isPlanet(WorldBodyId world_body_id) const
	{
		return findPlanet(world_body_id) != nullptr;
	}

	PlanetData* PlanetSystem::findPlanet(WorldBodyId world_body_id)
	{
		for(PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id) return &planet;
		return nullptr;
	}

	const PlanetData* PlanetSystem::findPlanet(WorldBodyId world_body_id) const
	{
		for(const PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id) return &planet;
		return nullptr;
	}

	PlanetData& PlanetSystem::getPlanet(WorldBodyId world_body_id)
	{
		PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr) danikk_engine::formatFatalError("WorldBody % is not registered as a planet", world_body_id);
		return *planet;
	}

	const PlanetData& PlanetSystem::getPlanet(WorldBodyId world_body_id) const
	{
		const PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr) danikk_engine::formatFatalError("WorldBody % is not registered as a planet", world_body_id);
		return *planet;
	}

	double PlanetSystem::getPlanetDensity(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return samplePlanetDensity(position, snapshot.planet_radius, snapshot.noise_seed,
			snapshot.terrain_edits, snapshot.core_radius);
	}

	MaterialId PlanetSystem::getPlanetMaterial(const PlanetTerrainSnapshot& snapshot, const glm::dvec3& position)
	{
		return samplePlanetMaterial(position, snapshot.planet_radius, snapshot.noise_seed);
	}

	double PlanetSystem::getPlanetDensity(WorldBodyId world_body_id, const glm::dvec3& position) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		return samplePlanetDensity(position, planet.radius, planet.noise_seed,
			planet.terrain_edits, planet.core_radius);
	}

	MaterialId PlanetSystem::getPlanetMaterial(WorldBodyId world_body_id, const glm::dvec3& position) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		return samplePlanetMaterial(position, planet.radius, planet.noise_seed);
	}

	double PlanetSystem::getPlanetHeightAboveSurface(WorldBodyId world_body_id, const glm::dvec3& position) const
	{
		const PlanetData& planet = getPlanet(world_body_id);
		const double distance_from_center = glm::length(position);
		if(distance_from_center <= 1e-9) return -planet.radius;
		const glm::dvec3 direction = position / distance_from_center;
		return distance_from_center - samplePlanetSurfaceRadius(direction, planet.radius, planet.noise_seed);
	}

	void PlanetSystem::frame()
	{
		DSE_WATCH();
		for(PlanetData& planet : planets) updatePlanet(planet);
	}

	void PlanetSystem::updatePlanet(PlanetData& planet)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)
			|| !world.camera.isActive()) return;
		if(!planet.terrain_edits_loaded)
		{
			loadPlanetTerrainEdits(planet.world_body_id, planet.radius, planet.noise_seed,
				planet.terrain_edits, planet.terrain_revision);
			planet.terrain_edits_loaded = true;
			planet.processed_edit_count = planet.terrain_edits.size();
		}
		const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
		const glm::dvec3 camera_position = world.camera.getWorldPosition() - body.position;
		const double distance_from_center = glm::length(camera_position);
		const bool in_chunk_range = distance_from_center
			<= planet.radius + planet.chunk_load_radius + planet_chunk_width;
		if(!in_chunk_range)
		{
			if(planet.chunk_updates_active)
			{
				planet.chunk_lookup.clearDesired();
				planet.pending_requests.clear();
				unloadChunksOutsideDesiredSet(planet);
				queuePlanetRequests(planet);
				planet.chunk_updates_active = false;
				planet.chunk_set_initialized = false;
			}
			return;
		}
		while(planet.processed_edit_count < planet.terrain_edits.size())
			invalidateChunksAroundEdit(planet, planet.terrain_edits[planet.processed_edit_count++]);
		const double moved_distance = glm::length(camera_position - planet.last_chunk_update_position);
		const bool update_chunks = !planet.chunk_set_initialized
			|| moved_distance > planet_voxel_size * 2.0
			|| planet.last_chunk_update_revision != planet.terrain_revision
			|| planet.last_chunk_update_mesh_generator != settings.planet_mesh_generator;
		if(update_chunks)
		{
			updatePlanetChunks(planet, camera_position);
			queuePlanetRequests(planet);
		}
	}

	bool PlanetSystem::editGround(WorldBodyId world_body_id, const glm::dvec3& position,
		double radius, bool adds_solid_terrain)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id) || radius <= 0.0) return false;
		PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr) return false;
		const WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		const glm::dvec3 center = glm::conjugate(body.rotation) * (position - body.position);
		planet->terrain_edits.pushMove(PlanetTerrainEdit{center, radius, adds_solid_terrain});
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		savePlanetTerrainEdits(world_body_id, planet->radius, planet->noise_seed,
			planet->terrain_edits, planet->terrain_revision);
		return true;
	}

}
