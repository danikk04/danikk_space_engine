#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/math/noise.h>
#include <danikk_space_engine/systems/planet/planet.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
		const PlanetSettings planet_settings[planet_settings_ids::count] =
		{
			{"Earth", default_planet_radius, default_planet_core_radius,
				planet_chunk_load_radius, 9.8, glm::vec3(0.2f, 0.4f, 0.8f), 100000.0, 0}
		};

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
			const DynamicArray<PlanetTerrainEdit>& edits, double minimum_wavelength = 0.0)
		{
			double surface_radius = samplePlanetSurfaceRadius(
				direction, radius, noise_seed, minimum_wavelength);
			for(const PlanetTerrainEdit& edit : edits)
			{
				const double along_ray = glm::dot(edit.center, direction);
				const double perpendicular_squared = glm::max(
					0.0, glm::dot(edit.center, edit.center) - along_ray * along_ray);
				if(perpendicular_squared >= edit.radius * edit.radius) continue;
				const double half_chord = std::sqrt(edit.radius * edit.radius - perpendicular_squared);
				const double outer_radius = along_ray + half_chord;
				const double inner_radius = glm::max(0.0, along_ray - half_chord);
				if(edit.adds_solid_terrain) surface_radius = glm::max(surface_radius, outer_radius);
				else if(surface_radius >= inner_radius && surface_radius <= outer_radius)
					surface_radius = inner_radius;
			}
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
		planet.atmosphere_color = settings.atmosphere_color;
		planet.atmosphere_height = settings.atmosphere_height;
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
		DynamicArray<ChunkMeshGenerationRequest> no_lod_requests;
		for(size_t index = 0; index < chunk_render_data.size(); ++index)
		{
			if(chunk_render_data[index].world_body_id != world_body_id) continue;
			world.chunk_mesh_generation.replacePlanetLodRequests(
				world_body_id, chunk_render_data[index].planet_generation_id, no_lod_requests);
			destroyPlanetLodMeshes(chunk_render_data[index]);
			for(size_t shift = index + 1; shift < chunk_render_data.size(); shift++)
				chunk_render_data[shift - 1] = static_cast<PlanetChunkRenderData&&>(chunk_render_data[shift]);
			chunk_render_data.resize(chunk_render_data.size() - 1);
			break;
		}

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

	double PlanetSystem::getPlanetSurfaceRadius(
		const PlanetTerrainSnapshot& snapshot, const glm::dvec3& direction, double minimum_wavelength)
	{
		const double direction_length = glm::length(direction);
		if(direction_length <= 1e-12) return 0.0;
		const glm::dvec3 radial_direction = direction / direction_length;
		double surface_radius = samplePlanetSurfaceRadius(
			radial_direction, snapshot.planet_radius, snapshot.noise_seed,
			snapshot.terrain_edits, minimum_wavelength);
		return surface_radius;
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
		return distance_from_center - samplePlanetSurfaceRadius(
			direction, planet.radius, planet.noise_seed, planet.terrain_edits);
	}

	glm::vec3 PlanetSystem::getAtmosphereBackgroundColor(const glm::dvec3& world_position) const
	{
		const PlanetData* nearest_planet = nullptr;
		double nearest_surface_distance = std::numeric_limits<double>::max();
		double nearest_surface_height = 0.0;
		for(const PlanetData& planet : planets)
		{
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)
				|| planet.atmosphere_height <= 0.0) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			const glm::dvec3 planet_position = glm::conjugate(body.rotation)
				* (world_position - body.position);
			const double surface_height = getPlanetHeightAboveSurface(planet.world_body_id, planet_position);
			const double surface_distance = std::abs(surface_height);
			if(surface_distance >= nearest_surface_distance) continue;
			nearest_surface_distance = surface_distance;
			nearest_surface_height = surface_height;
			nearest_planet = &planet;
		}
		if(nearest_planet == nullptr || nearest_surface_height < 0.0) return glm::vec3(0.0f);

		const double atmosphere_factor = 1.0 - glm::smoothstep(
			0.0, nearest_planet->atmosphere_height, nearest_surface_height);
		return nearest_planet->atmosphere_color * static_cast<float>(atmosphere_factor);
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
		const double surface_height = getPlanetHeightAboveSurface(planet.world_body_id, camera_position);
		const double chunk_streaming_limit = planet_chunk_width
			* (planet_volume_chunk_half_width + 2.0);
		const bool in_chunk_range = surface_height <= chunk_streaming_limit;
		if(!in_chunk_range)
		{
			if(planet.chunk_updates_active)
			{
				danikk_framework::formatLogInfo(
					"Planet chunk streaming stopped: body=% height_above_surface=% m limit=% m reason=outside surface range",
					planet.world_body_id, surface_height, chunk_streaming_limit);
				planet.chunk_lookup.clearDesired();
				planet.pending_requests.clear();
				unloadChunksOutsideDesiredSet(planet);
				queuePlanetRequests(planet);
				planet.chunk_updates_active = false;
				planet.chunk_set_initialized = false;
			}
			return;
		}
		if(!planet.chunk_updates_active)
			danikk_framework::formatLogInfo(
				"Planet chunk streaming started: body=% height_above_surface=% m center_distance=% m",
				planet.world_body_id, surface_height, distance_from_center);
		while(planet.processed_edit_count < planet.terrain_edits.size())
			invalidateChunksAroundEdit(planet, planet.terrain_edits[planet.processed_edit_count++]);
		const glm::ivec3 camera_grid_coordinate(
			static_cast<int>(std::floor(camera_position.x / planet_chunk_width)),
			static_cast<int>(std::floor(camera_position.y / planet_chunk_width)),
			static_cast<int>(std::floor(camera_position.z / planet_chunk_width)));
		const bool update_chunks = !planet.chunk_set_initialized
			|| !glm::all(glm::equal(camera_grid_coordinate, planet.last_chunk_update_grid_coordinate))
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
