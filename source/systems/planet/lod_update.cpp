#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		void validateLodTriangleBudgets()
		{
			for(uint8 level = 1; level < planet_chunk_level_count; level++)
			{
				const PlanetLodSettings& settings = getPlanetLodSettings(level);
				const uint64 estimated_triangles = static_cast<uint64>(settings.patch_count)
					* settings.resolution * settings.resolution * 2;
				if(estimated_triangles > settings.triangle_budget)
					danikk_engine::formatFatalError(
						"Planet LOD level % exceeds triangle budget: estimated=% budget=%",
						level, estimated_triangles, settings.triangle_budget);
			}
		}
	}

	void PlanetSystem::initializePlanetLodLevels(PlanetData& planet)
	{
		DSE_WATCH();
		if(planet.levels.size() != 0) return;
		validateLodTriangleBudgets();
		planet.levels.resize(planet_chunk_level_count);
		for(uint8 level_index = 1; level_index < planet_chunk_level_count; level_index++)
		{
			PlanetChunkLevel& level = planet.levels[level_index];
			const PlanetLodSettings& settings = getPlanetLodSettings(level_index);
			level.geometry = settings.geometry;
			level.resolution = settings.resolution;
			level.expected_patch_count = settings.patch_count;
			level.camera_local = settings.coverage == PlanetLodCoverage::fixed_diameter
				|| settings.coverage == PlanetLodCoverage::horizon;
			level.patches.resize(settings.patch_count);
			if(settings.coverage == PlanetLodCoverage::whole_planet)
			{
				for(uint8 face = 0; face < planet_lod_face_count; face++)
					level.patches[face].grid_coordinate = glm::ivec3(face, level_index, 0);
			}
		}
	}

	/** @brief Updates each planet's selected LOD and queues missing surface meshes. */
	void PlanetSystem::updateLod()
	{
		DSE_WATCH();
		for(PlanetData& planet : planets)
		{
			if(!world.world_body_data.isWorldBodyActive(planet.world_body_id))
			{
				DynamicArray<ChunkMeshGenerationRequest> no_requests;
				world.chunk_mesh_generation.replacePlanetLodRequests(
					planet.world_body_id, planet.generation_id, no_requests);
				destroyPlanetLodMeshes(planet);
				planet.lod_enabled = false;
				planet.target_lod_level = 0;
				continue;
			}
			updatePlanetLod(planet);
		}
	}

	void PlanetSystem::updatePlanetLod(PlanetData& planet)
	{
		DSE_WATCH();
		initializePlanetLodLevels(planet);
		const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
		PlanetChunkLevel& regular_level = planet.levels[0];
		regular_level.chunk_ids.clear();
		const WorldBody& body = world.world_body_data.getWorldBody(planet.world_body_id);
		for(ChunkId chunk_id : body.chunk_ids) regular_level.chunk_ids.push(chunk_id);
		if(planet.lod_terrain_revision != planet.terrain_revision)
		{
			destroyPlanetLodMeshes(planet);
			planet.lod_terrain_revision = planet.terrain_revision;
		}

		DynamicArray<ChunkMeshGenerationRequest> requests;
		if(!world.camera.isActive())
		{
			planet.lod_enabled = false;
			planet.lod_level = 0;
			planet.target_lod_level = 0;
		}
		else
		{
			const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
				* (world.camera.getWorldPosition() - body.position);
			const double height = getPlanetHeightAboveSurface(
				planet.world_body_id, local_camera_position);
			const bool was_enabled = planet.lod_enabled;
			const PlanetLodSettings& first_lod_settings = getPlanetLodSettings(1);
			const double lod_activation_height = first_lod_settings.getActivationHeight(settings.radius);
			const double lod_deactivation_height = first_lod_settings.getDeactivationHeight(settings.radius);
			if(planet.lod_enabled)
			{
				if(height < lod_deactivation_height) planet.lod_enabled = false;
			}
			else if(height > lod_activation_height) planet.lod_enabled = true;
			if(planet.lod_enabled != was_enabled)
			{
				if(planet.lod_enabled)
					danikk_framework::formatLogInfo(
						"Planet LOD enabled: body=% height_above_surface=% m activation_threshold=% m",
						planet.world_body_id, height, lod_activation_height);
				else
					danikk_framework::formatLogInfo(
						"Planet LOD disabled: body=% height_above_surface=% m deactivation_threshold=% m reason=below surface coverage range",
						planet.world_body_id, height, lod_deactivation_height);
			}

			if(planet.lod_enabled)
			{
				const uint8 previous_level = planet.lod_level;
				const uint8 target_level = selectPlanetLodTargetLevel(planet, height, settings);
				queuePlanetLodRequests(planet, target_level, local_camera_position,
					settings, requests);
				planet.lod_level = selectPlanetLodRenderLevel(planet, target_level);
				if(planet.lod_level != previous_level)
					danikk_framework::formatLogInfo(
						"Planet surface render level changed: body=% level=% height_above_surface=% m",
						planet.world_body_id, planet.lod_level, height);
			}
			else
			{
				planet.lod_level = 0;
				planet.target_lod_level = 0;
			}
		}
		world.chunk_mesh_generation.replacePlanetLodRequests(
			planet.world_body_id, planet.generation_id, requests);
	}
}
