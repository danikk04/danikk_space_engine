#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void PlanetSystem::initializePlanetLodLevels(PlanetData& planet)
	{
		DSE_WATCH();
		if(planet.levels.size() != 0) return;
		planet.levels.resize(planet_chunk_level_count);
		planet.levels[0].geometry = PlanetChunkLevelGeometry::regular_volume_chunks;
		for(uint8 level_index = 1; level_index < planet_chunk_level_count; level_index++)
		{
			PlanetChunkLevel& level = planet.levels[level_index];
			if(level_index == 1)
			{
				level.geometry = PlanetChunkLevelGeometry::local_heightmap;
				level.resolution = 16;
				level.expected_patch_count = planet_local_heightmap_tile_count;
				level.camera_local = true;
				level.patches.resize(planet_local_heightmap_tile_count);
			}
			else if(level_index <= 3)
			{
				level.geometry = PlanetChunkLevelGeometry::local_heightmap;
				level.resolution = 64;
				level.expected_patch_count = planet_local_heightmap_tile_count;
				level.camera_local = true;
				level.patches.resize(planet_local_heightmap_tile_count);
			}
			else
			{
				level.geometry = PlanetChunkLevelGeometry::global_heightmap;
				level.resolution = level_index == 4 ? 29 : 9;
				level.expected_patch_count = planet_lod_face_count;
				level.camera_local = false;
				level.patches.resize(planet_lod_face_count);
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
		}
		else
		{
			const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
				* (world.camera.getWorldPosition() - body.position);
			const double height = getPlanetHeightAboveSurface(
				planet.world_body_id, local_camera_position);
			const bool was_enabled = planet.lod_enabled;
			if(planet.lod_enabled)
			{
				if(height < 70.0) planet.lod_enabled = false;
			}
			else if(height > 100.0) planet.lod_enabled = true;
			if(planet.lod_enabled != was_enabled)
			{
				if(planet.lod_enabled)
					danikk_framework::formatLogInfo(
						"Planet LOD enabled: body=% height_above_surface=% m activation_threshold=100 m",
						planet.world_body_id, height);
				else
					danikk_framework::formatLogInfo(
						"Planet LOD disabled: body=% height_above_surface=% m deactivation_threshold=70 m reason=below surface coverage range",
						planet.world_body_id, height);
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
			}
		}
		world.chunk_mesh_generation.replacePlanetLodRequests(
			planet.world_body_id, planet.generation_id, requests);
	}
}
