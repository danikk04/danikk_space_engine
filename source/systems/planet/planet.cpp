#include <cmath>
#include <limits>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/planet_render/system.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/systems/world_body/world_body.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/save/system.h>

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
	}

	bool PlanetSystem::removePlanetTerrainEdit(PlanetId planet_id, PlanetTerrainEditId edit_id)
	{
		PlanetData* planet = findPlanet(planet_id);
		if(planet == nullptr || !planet->terrain_edits.pool->isActive(edit_id)) return false;
		const PlanetTerrainEdit& edit = (*planet->terrain_edits.pool)[edit_id];
		const PlanetTerrainDirtyRegion dirty_region{edit.getCenter(), edit.getRadius()};
		if(!planet->terrain_edits.remove(edit_id)) return false;
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.getPlanetRenderSystem().markTerrainDirty(planet_id, dirty_region);
		world.getSaveSystem().savePlanetTerrainEdits(planet_id);
		return true;
	}

	const PlanetSettings& PlanetSystem::getPlanetSettings(PlanetSettingsId id) const
	{
		if(id >= planet_settings_ids::count)
			danikk_engine::formatFatalError("Invalid planet settings ID %", id);
		return planet_settings[id];
	}

	void PlanetSystem::updateRenderDistances(const glm::dvec3& camera_position, double camera_far_clip_distance, double& far_render_distance, double& closest_planet_surface_distance) const
	{
		far_render_distance = camera_far_clip_distance;
		closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : planets)
		{
			const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(planet.world_body_id);
			const glm::dvec3 local_camera = glm::conjugate(body.rotation) * (camera_position - body.position);
			const double surface_height = getPlanetHeightAboveSurface(planet.id, local_camera);
			closest_planet_surface_distance = glm::min(closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance, glm::length(body.position - camera_position) + settings.radius);
		}
	}

	PlanetData& PlanetSystem::createPlanet(PlanetSettingsId settings_id,
		WorldBodyId world_body_id)
	{
		if(!world.getWorldBodySystem().isWorldBodyActive(world_body_id))
			danikk_engine::formatFatalError("Cannot attach a planet to inactive WorldBody %", world_body_id);
		if(findPlanetByWorldBodyId(world_body_id) != nullptr)
			danikk_engine::formatFatalError("WorldBody % already owns a planet", world_body_id);
		PlanetData planet(settings_id);
		planet.id = 0;
		while(findPlanet(planet.id) != nullptr) ++planet.id;
		planet.world_body_id = world_body_id;
		planets.pushMove(planet);
		world.getSaveSystem().loadPlanetTerrainEdits(planet.id);
		return planets[planets.size() - 1];
	}

	void PlanetSystem::destroyPlanet(PlanetId planet_id)
	{
		// TODO: Wait for the planet's active mesh-generation jobs before erasing its PlanetData.
		// Workers sample PlanetSystem while generating, so removal is not safe until they finish.
		for(size_t index = 0; index < planets.size(); ++index)
		{
			if(planets[index].id != planet_id) continue;
			world.getPlanetRenderSystem().destroyPlanet(planet_id);
			for(size_t shift = index + 1; shift < planets.size(); ++shift)
				planets[shift - 1] = static_cast<PlanetData&&>(planets[shift]);
			planets.pop();
			return;
		}
	}

	bool PlanetSystem::isPlanet(PlanetId planet_id) const
	{
		return findPlanet(planet_id) != nullptr;
	}

	PlanetData* PlanetSystem::findPlanet(PlanetId planet_id)
	{
		for(PlanetData& planet : planets)
			if(planet.id == planet_id) return &planet;
		return nullptr;
	}

	const PlanetData* PlanetSystem::findPlanet(PlanetId planet_id) const
	{
		for(const PlanetData& planet : planets)
			if(planet.id == planet_id) return &planet;
		return nullptr;
	}

	PlanetData* PlanetSystem::findPlanetByWorldBodyId(WorldBodyId world_body_id)
	{
		for(PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id) return &planet;
		return nullptr;
	}

	const PlanetData* PlanetSystem::findPlanetByWorldBodyId(WorldBodyId world_body_id) const
	{
		for(const PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id) return &planet;
		return nullptr;
	}

	bool PlanetSystem::isPlanetWorldBody(WorldBodyId world_body_id) const
	{
		return findPlanetByWorldBodyId(world_body_id) != nullptr;
	}

	void PlanetSystem::destroyPlanetForWorldBody(WorldBodyId world_body_id)
	{
		for(const PlanetData& planet : planets)
			if(planet.world_body_id == world_body_id)
			{
				destroyPlanet(planet.id);
				return;
			}
	}

	PlanetData& PlanetSystem::getPlanet(PlanetId planet_id)
	{
		PlanetData* planet = findPlanet(planet_id);
		if(planet == nullptr) danikk_engine::formatFatalError("Planet % is not registered", planet_id);
		return *planet;
	}

	const PlanetData& PlanetSystem::getPlanet(PlanetId planet_id) const
	{
		const PlanetData* planet = findPlanet(planet_id);
		if(planet == nullptr) danikk_engine::formatFatalError("Planet % is not registered", planet_id);
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
			if(settings.atmosphere_height <= 0.0) continue;
			const WorldBody& body = world.getWorldBodySystem().getWorldBody(planet.world_body_id);
			const glm::dvec3 planet_position = glm::conjugate(body.rotation)
				* (world_position - body.position);
			const double surface_height = getPlanetHeightAboveSurface(planet.id, planet_position);
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

	bool PlanetSystem::editPlanetGround(PlanetId planet_id, const glm::dvec3& position,
		double radius, MaterialId material_id)
	{
		constexpr bool enable_terrain_edit_system_logs = true;
		if(!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
			|| !std::isfinite(radius) || radius <= 0.0
			|| (material_id != default_materials::void_material && material_id >= getMaterialCount()))
		{
			if constexpr(enable_terrain_edit_system_logs)
				danikk_framework::formatLogWarning(
					"Planet terrain edit rejected: invalid arguments planet=% radius=% material=%",
					planet_id, radius, material_id);
			return false;
		}
		PlanetData* planet = findPlanet(planet_id);
		if(planet == nullptr)
		{
			if constexpr(enable_terrain_edit_system_logs)
				danikk_framework::formatLogWarning("Planet terrain edit rejected: missing planet=%", planet_id);
			return false;
		}
		const WorldBody& body = world.getWorldBodySystem().getWorldBody(planet->world_body_id);
		const glm::dvec3 center = glm::conjugate(body.rotation) * (position - body.position);
		if(planet->terrain_edits.append(PlanetTerrainEdit{center, radius, material_id})
			== static_cast<PlanetTerrainEditId>(-1))
		{
			if constexpr(enable_terrain_edit_system_logs)
				danikk_framework::formatLogError(
					"Planet terrain edit rejected: octree append failed planet=% center=(%,%,%) radius=%",
					planet_id, center.x, center.y, center.z, radius);
			return false;
		}
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.getPlanetRenderSystem().markTerrainDirty(planet_id,
			PlanetTerrainDirtyRegion{center, radius});
		if constexpr(enable_terrain_edit_system_logs)
			danikk_framework::formatLogInfo(
				"Planet terrain edit accepted: planet=% revision=% center=(%,%,%) radius=% material=%",
				planet_id, planet->terrain_revision, center.x, center.y, center.z, radius, material_id);
		world.getSaveSystem().savePlanetTerrainEdits(planet_id);
		return true;
	}

}
