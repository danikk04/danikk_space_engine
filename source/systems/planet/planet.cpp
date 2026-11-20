#include <cmath>
#include <limits>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
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

	}

	const PlanetSettings& PlanetSystem::getPlanetSettings(PlanetSettingsId id) const
	{
		if(id >= planet_settings_ids::count)
			danikk_engine::formatFatalError("Invalid planet settings ID %", id);
		return planet_settings[id];
	}

	PlanetData& PlanetSystem::createPlanet(WorldBodyId world_body_id, PlanetSettingsId settings_id)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Cannot register a planet for inactive WorldBody %", world_body_id);
		getPlanetSettings(settings_id);
		if(PlanetData* existing = findPlanet(world_body_id)) return *existing;
		PlanetData planet;
		planet.world_body_id = world_body_id;
		planet.settings_id = settings_id;
		planet.generation_id = next_generation_id++;
		if(next_generation_id == 0) next_generation_id = 1;
		planets.pushMove(planet);
		world.saves.loadPlanetTerrainEdits(world_body_id);
		return planets[planets.size() - 1];
	}

	void PlanetSystem::removeQueuedRequests(const PlanetData& planet)
	{
		world.chunk_mesh_generation.cancelPlanetRequests(planet.world_body_id, planet.generation_id);
	}

	void PlanetSystem::destroyPlanet(WorldBodyId world_body_id)
	{
		DynamicArray<ChunkMeshGenerationRequest> no_lod_requests;
		for(size_t index = 0; index < planets.size(); ++index)
		{
			if(planets[index].world_body_id != world_body_id) continue;
			world.chunk_mesh_generation.replacePlanetLodRequests(
				world_body_id, planets[index].generation_id, no_lod_requests);
			destroyPlanetLodMeshes(planets[index]);
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

	glm::vec3 PlanetSystem::getAtmosphereBackgroundColor(const glm::dvec3& world_position) const
	{
		const PlanetData* nearest_planet = nullptr;
		double nearest_surface_distance = std::numeric_limits<double>::max();
		double nearest_surface_height = 0.0;
		const PlanetSettings* nearest_settings = nullptr;
		for(const PlanetData& planet : planets)
		{
			const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)
				|| settings.atmosphere_height <= 0.0) continue;
			const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
			const glm::dvec3 planet_position = glm::conjugate(body.rotation)
				* (world_position - body.position);
			const double surface_height = getPlanetHeightAboveSurface(planet.world_body_id, planet_position);
			const double surface_distance = std::abs(surface_height);
			if(surface_distance >= nearest_surface_distance) continue;
			nearest_surface_distance = surface_distance;
			nearest_surface_height = surface_height;
			nearest_planet = &planet;
			nearest_settings = &settings;
		}
		if(nearest_planet == nullptr || nearest_settings == nullptr || nearest_surface_height < 0.0)
			return glm::vec3(0.0f);

		const double atmosphere_factor = 1.0 - glm::smoothstep(
			0.0, nearest_settings->atmosphere_height, nearest_surface_height);
		return nearest_settings->atmosphere_color * static_cast<float>(atmosphere_factor);
	}

	void PlanetSystem::update()
	{
		DSE_WATCH();
		for(PlanetData& planet : planets) updatePlanet(planet);
		updateLod();
	}

	void PlanetSystem::updatePlanet(PlanetData& planet)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)
			|| !world.camera.isActive()) return;
		const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
		const glm::dvec3 camera_position = glm::conjugate(body.rotation)
			* (world.camera.getWorldPosition() - body.position);
		const double distance_from_center = glm::length(camera_position);
		const double surface_height = getPlanetHeightAboveSurface(planet.world_body_id, camera_position);
		glm::dvec3 chunk_center_position = camera_position;
		uint8 lod_level = 0;
		getCurrentLodLevel(planet.world_body_id, lod_level);
		if(surface_height >= 0.0 && lod_level == 0 && distance_from_center > 1e-9)
			chunk_center_position -= camera_position / distance_from_center * surface_height;
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
		while(planet.processed_edit_count < planet.terrain_edits.ids.size())
			invalidateChunksAroundEdit(planet,
				(*planet.terrain_edits.pool)[planet.terrain_edits.ids[planet.processed_edit_count++]]);
		const glm::ivec3 chunk_center_grid_coordinate(
			static_cast<int>(std::floor(chunk_center_position.x / planet_chunk_width)),
			static_cast<int>(std::floor(chunk_center_position.y / planet_chunk_width)),
			static_cast<int>(std::floor(chunk_center_position.z / planet_chunk_width)));
		const bool update_chunks = !planet.chunk_set_initialized
			|| !glm::all(glm::equal(chunk_center_grid_coordinate, planet.last_chunk_update_grid_coordinate))
			|| planet.last_chunk_update_revision != planet.terrain_revision
			|| planet.last_chunk_update_mesh_generator != settings.planet_mesh_generator;
		if(update_chunks)
		{
			updatePlanetChunks(planet, camera_position, chunk_center_position);
			queuePlanetRequests(planet);
		}
	}

	bool PlanetSystem::editPlanetGround(WorldBodyId world_body_id, const glm::dvec3& position,
		double radius, MaterialId material_id)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id) || radius <= 0.0
			|| (material_id != default_materials::void_material && material_id >= getMaterialCount())) return false;
		PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr) return false;
		const WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		const glm::dvec3 center = glm::conjugate(body.rotation) * (position - body.position);
		planet->terrain_edits.append(PlanetTerrainEdit{center, radius, material_id}, planet_chunk_width);
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.saves.savePlanetTerrainEdits(world_body_id);
		return true;
	}

}
