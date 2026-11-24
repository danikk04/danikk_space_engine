#include <algorithm>
#include <cmath>

#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/planet_render/system.h>
#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	PlanetRenderData::PlanetRenderData(const PlanetData& planet,
		const PlanetSettings& settings, uint64 runtime_generation_id)
		: planet_id(planet.id), runtime_generation_id(runtime_generation_id),
		  terrain_tree_revision(planet.terrain_revision), terrain_tree(settings)
	{
	}

	PlanetRenderData* PlanetRenderSystem::findPlanetRenderData(PlanetId planet_id)
	{
		for(PlanetRenderData& data : planet_render_data)
			if(data.planet_id == planet_id) return &data;
		return nullptr;
	}

	const PlanetRenderData* PlanetRenderSystem::findPlanetRenderData(PlanetId planet_id) const
	{
		for(const PlanetRenderData& data : planet_render_data)
			if(data.planet_id == planet_id) return &data;
		return nullptr;
	}

	bool PlanetRenderSystem::isPlanetRenderRelevant(const PlanetData& planet,
		const glm::dvec3& camera_position, const glm::dvec3& camera_front,
		double vertical_fov_degrees, double& surface_height) const
	{
		const PlanetSettings& planet_settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
		const glm::dvec3 local_camera_position = glm::conjugate(planet.rotation)
			* (camera_position - planet.position);
		surface_height = world.getPlanetSystem().getPlanetHeightAboveSurface(
			planet.id, local_camera_position);
		const double nearby_activation_distance = planet_settings.atmosphere_height
			+ world.getCameraSystem().far_clip_distance;
		if(std::abs(surface_height) <= nearby_activation_distance) return true;

		const glm::dvec3 to_planet = planet.position - camera_position;
		const double center_distance = glm::length(to_planet);
		if(center_distance <= planet_settings.radius) return true;
		const double angular_radius = std::asin(glm::clamp(
			planet_settings.radius / center_distance, 0.0, 1.0));
		if(angular_radius * 2.0 < 0.01) return false;

		const glm::dvec3 direction = to_planet / center_distance;
		const double camera_front_length = glm::length(camera_front);
		if(camera_front_length <= 1e-9) return false;
		const glm::dvec3 normalized_front = camera_front / camera_front_length;
		const double center_angle = std::acos(glm::clamp(
			glm::dot(direction, normalized_front), -1.0, 1.0));
		const double aspect = glm::max(static_cast<double>(screen_ratio_gz), 0.1);
		const double half_diagonal_fov = std::atan(
			std::tan(glm::radians(vertical_fov_degrees) * 0.5)
			* std::sqrt(1.0 + aspect * aspect));
		return center_angle <= half_diagonal_fov + angular_radius;
	}

	PlanetRenderData& PlanetRenderSystem::ensurePlanetRenderData(const PlanetData& planet)
	{
		PlanetRenderData* existing = findPlanetRenderData(planet.id);
		if(existing != nullptr) return *existing;
		const PlanetSettings& planet_settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
		const uint64 runtime_generation_id = next_runtime_generation_id++;
		if(next_runtime_generation_id == 0) next_runtime_generation_id = 1;
		planet_render_data.pushMove(PlanetRenderData(planet, planet_settings, runtime_generation_id));
		return planet_render_data[planet_render_data.size() - 1];
	}

	void PlanetRenderSystem::updatePlanet(const PlanetData& planet,
		PlanetRenderData& render_data, double surface_height)
	{
		const glm::dvec3 camera_position = glm::conjugate(planet.rotation)
			* (world.getCameraSystem().getWorldPosition() - planet.position);
		const PlanetSettings& planet_settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
		render_data.updateTerrainStreaming(planet, camera_position, surface_height,
			planet_settings, world.getTelemetrySystem().mesh_generation_telemetry);
		render_data.active_this_frame = true;
		evictCachedMeshes(render_data, camera_position);
	}

	void PlanetRenderSystem::update()
	{
		DSE_WATCH();
		for(PlanetRenderData& data : planet_render_data) data.active_this_frame = false;
		const CameraSystem& camera = world.getCameraSystem();
		if(camera.isActive())
		{
			const glm::dvec3 camera_position = camera.getWorldPosition();
			const glm::dvec3 camera_front(camera.getFront());
			const DynamicArray<PlanetData>& planets = world.getPlanetSystem().getPlanets();
			for(const PlanetData& planet : planets)
			{
				double surface_height = 0.0;
				if(!isPlanetRenderRelevant(planet, camera_position,
					camera_front, camera.vertical_fov_degrees, surface_height)) continue;
				PlanetRenderData& render_data = ensurePlanetRenderData(planet);
				updatePlanet(planet, render_data, surface_height);
			}
		}

		for(size_t index = planet_render_data.size(); index > 0; --index)
			if(!planet_render_data[index - 1].active_this_frame)
				removePlanetRenderData(index - 1, MeshDeletionReason::planet_render_unloaded);
	}

	void PlanetRenderSystem::removePlanetRenderData(size_t index,
		MeshDeletionReason reason)
	{
		if(index >= planet_render_data.size()) return;
		PlanetRenderData& data = planet_render_data[index];
		world.getMeshGenerationSystem().cancelPlanetJobs(data.planet_id);
		data.destroyTerrainTree(world.getTelemetrySystem().mesh_generation_telemetry, reason);
		for(size_t shift = index + 1; shift < planet_render_data.size(); ++shift)
			planet_render_data[shift - 1] = static_cast<PlanetRenderData&&>(planet_render_data[shift]);
		planet_render_data.pop();
	}

	void PlanetRenderSystem::destroyPlanet(PlanetId planet_id)
	{
		for(size_t index = 0; index < planet_render_data.size(); ++index)
			if(planet_render_data[index].planet_id == planet_id)
			{
				removePlanetRenderData(index, MeshDeletionReason::planet_removed);
				return;
			}
	}

	void PlanetRenderSystem::markTerrainDirty(PlanetId planet_id,
		const PlanetTerrainDirtyRegion& region)
	{
		PlanetRenderData* data = findPlanetRenderData(planet_id);
		if(data != nullptr) data->pending_terrain_dirty_regions.push(region);
	}

	void PlanetRenderSystem::shutdown()
	{
		while(planet_render_data.size() > 0)
			removePlanetRenderData(planet_render_data.size() - 1,
				MeshDeletionReason::planet_system_shutdown);
	}
}
