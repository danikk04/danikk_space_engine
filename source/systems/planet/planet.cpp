#include <cmath>
#include <limits>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
		const PlanetSettings planet_settings[planet_settings_ids::count] =
		{
			{"Earth", default_planet_radius, default_planet_core_radius,
				9.8, glm::vec3(0.2f, 0.4f, 0.8f), 100000.0, 0}
		};

	}

	void PlanetSystem::init()
	{
		danikk_framework::formatLogInfo("Init PlanetSystem");
	}

	void PlanetSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown PlanetSystem");
		for(PlanetData& planet : planets)
		{
			removeQueuedRequests(planet);
			destroyPlanetTerrainTree(planet);
		}
	}

	bool PlanetSystem::removePlanetTerrainEdit(WorldBodyId world_body_id, PlanetTerrainEditId edit_id)
	{
		PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr || !planet->terrain_edits.remove(edit_id)) return false;
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.saves.savePlanetTerrainEdits(world_body_id);
		return true;
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
		world.mesh_generation.cancelPlanetRequests(planet.world_body_id, planet.generation_id);
	}

	void PlanetSystem::destroyPlanet(WorldBodyId world_body_id)
	{
		for(size_t index = 0; index < planets.size(); ++index)
		{
			if(planets[index].world_body_id != world_body_id) continue;
			removeQueuedRequests(planets[index]);
			destroyPlanetTerrainTree(planets[index]);
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
	}

	void PlanetSystem::updatePlanet(PlanetData& planet)
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(planet.world_body_id)
			|| !world.camera.isActive()) return;
		const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
		const glm::dvec3 camera_position = glm::conjugate(body.rotation)
			* (world.camera.getWorldPosition() - body.position);
		const double surface_height = getPlanetHeightAboveSurface(planet.world_body_id, camera_position);
		updatePlanetTerrainTree(planet, camera_position, surface_height);
	}

	bool PlanetSystem::editPlanetGround(WorldBodyId world_body_id, const glm::dvec3& position,
		double radius, MaterialId material_id)
	{
		if(!world.world_body_data.isWorldBodyActive(world_body_id)
			|| !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
			|| !std::isfinite(radius) || radius <= 0.0
			|| (material_id != default_materials::void_material && material_id >= getMaterialCount())) return false;
		PlanetData* planet = findPlanet(world_body_id);
		if(planet == nullptr) return false;
		const WorldBody& body = world.world_body_data.getWorldBody(world_body_id);
		const glm::dvec3 center = glm::conjugate(body.rotation) * (position - body.position);
		if(planet->terrain_edits.append(PlanetTerrainEdit{center, radius, material_id})
			== static_cast<PlanetTerrainEditId>(-1)) return false;
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.saves.savePlanetTerrainEdits(world_body_id);
		return true;
	}

}
