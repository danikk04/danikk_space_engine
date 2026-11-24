#include <cmath>
#include <limits>
#include <optional>

#include <danikk_framework/log.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/camera/camera.h>
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
		for(PlanetData& planet : planets)
		{
			cancelQueuedJobsForPlanet(planet);
			planet.destroyTerrainTree(world.getTelemetrySystem().mesh_generation_telemetry,
				MeshDeletionReason::planet_system_shutdown);
		}
	}

	bool PlanetSystem::removePlanetTerrainEdit(PlanetId planet_id, PlanetTerrainEditId edit_id)
	{
		PlanetData* planet = findPlanet(planet_id);
		if(planet == nullptr || !planet->terrain_edits.pool->isActive(edit_id)) return false;
		const PlanetTerrainEdit& edit = (*planet->terrain_edits.pool)[edit_id];
		const PlanetTerrainDirtyRegion dirty_region{edit.getCenter(), edit.getRadius()};
		if(!planet->terrain_edits.remove(edit_id)) return false;
		planet->pending_terrain_dirty_regions.push(dirty_region);
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		world.getSaveSystem().savePlanetTerrainEdits(planet_id);
		return true;
	}

	const PlanetSettings& PlanetSystem::getPlanetSettings(PlanetSettingsId id) const
	{
		if(id >= planet_settings_ids::count)
			danikk_engine::formatFatalError("Invalid planet settings ID %", id);
		return planet_settings[id];
	}

	PlanetData& PlanetSystem::createPlanet(PlanetSettingsId settings_id,
		const glm::dvec3& position, const glm::dquat& rotation)
	{
		PlanetData planet(settings_id, getPlanetSettings(settings_id));
		planet.id = 0;
		while(findPlanet(planet.id) != nullptr) ++planet.id;
		planet.position = position;
		planet.rotation = rotation;
		planet.generation_id = next_generation_id++;
		if(next_generation_id == 0) next_generation_id = 1;
		planets.pushMove(planet);
		world.getSaveSystem().loadPlanetTerrainEdits(planet.id);
		return planets[planets.size() - 1];
	}

	void PlanetSystem::cancelQueuedJobsForPlanet(const PlanetData& planet)
	{
		world.getMeshGenerationSystem().cancelPlanetJobs(planet.id, planet.generation_id);
	}

	void PlanetSystem::destroyPlanet(PlanetId planet_id)
	{
		for(size_t index = 0; index < planets.size(); ++index)
		{
			if(planets[index].id != planet_id) continue;
			cancelQueuedJobsForPlanet(planets[index]);
			planets[index].destroyTerrainTree(
				world.getTelemetrySystem().mesh_generation_telemetry,
				MeshDeletionReason::planet_removed);
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
			const glm::dvec3 planet_position = glm::conjugate(planet.rotation)
				* (world_position - planet.position);
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

	void PlanetSystem::update()
	{
		DSE_WATCH();
		for(PlanetData& planet : planets) updatePlanet(planet);
	}

	void PlanetSystem::updatePlanet(PlanetData& planet)
	{
		DSE_WATCH();
		if(!world.getCameraSystem().isActive())
		{
			planet.terrain_streaming_active = false;
			return;
		}
		const glm::dvec3 camera_position = glm::conjugate(planet.rotation)
			* (world.getCameraSystem().getWorldPosition() - planet.position);
		const double surface_height = getPlanetHeightAboveSurface(planet.id, camera_position);
		planet.updateTerrainStreaming(camera_position, surface_height,
			getPlanetSettings(planet.settings_id),
			world.getTelemetrySystem().mesh_generation_telemetry);
		evictCachedMeshes(planet.terrain_tree, camera_position);
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
		const glm::dvec3 center = glm::conjugate(planet->rotation) * (position - planet->position);
		if(planet->terrain_edits.append(PlanetTerrainEdit{center, radius, material_id})
			== static_cast<PlanetTerrainEditId>(-1))
		{
			if constexpr(enable_terrain_edit_system_logs)
				danikk_framework::formatLogError(
					"Planet terrain edit rejected: octree append failed planet=% center=(%,%,%) radius=%",
					planet_id, center.x, center.y, center.z, radius);
			return false;
		}
		planet->pending_terrain_dirty_regions.push(PlanetTerrainDirtyRegion{center, radius});
		planet->terrain_revision++;
		if(planet->terrain_revision == 0) planet->terrain_revision++;
		if constexpr(enable_terrain_edit_system_logs)
			danikk_framework::formatLogInfo(
				"Planet terrain edit accepted: planet=% revision=% center=(%,%,%) radius=% material=%",
				planet_id, planet->terrain_revision, center.x, center.y, center.z, radius, material_id);
		world.getSaveSystem().savePlanetTerrainEdits(planet_id);
		return true;
	}

}
