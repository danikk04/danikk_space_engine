#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/planet.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 lod_resolutions[planet_lod_level_count] = {200, 256, 128, 64, 32, 16};
		constexpr double level_altitude_ratios[planet_lod_level_count - 1] = {0.001, 0.01, 0.05, 0.25, 0.75};
		constexpr double lod_activation_height = 100.0;
		constexpr double lod_deactivation_height = 70.0;
		constexpr double local_surface_patch_width = planet_chunk_width * 16.0;
		constexpr uint8 local_patch_level = 1;
		constexpr uint8 local_patch_face = 0;
		constexpr uint32 global_face_patch_count = planet_lod_face_count;

std::shared_ptr<const PlanetTerrainSnapshot> createLodSnapshot(const PlanetData& planet)
		{
			std::shared_ptr<PlanetTerrainSnapshot> snapshot = std::make_shared<PlanetTerrainSnapshot>();
			snapshot->world_body_id = planet.world_body_id;
			snapshot->planet_generation_id = planet.generation_id;
			snapshot->terrain_revision = planet.terrain_revision;
			snapshot->planet_radius = planet.radius;
			snapshot->core_radius = planet.core_radius;
			snapshot->noise_seed = planet.noise_seed;
			snapshot->terrain_edits = planet.terrain_edits;
			return snapshot;
		}

bool isLevelReady(const PlanetChunkLevel& level)
		{
			if(!level.initialized || level.mesh == nullptr
				|| level.patches.size() != (level.camera_local ? 1 : planet_lod_face_count)) return false;
			for(const PlanetLodPatch& patch : level.patches)
				if(!patch.ready) return false;
			return true;
		}
	}

	PlanetChunkRenderData& PlanetSystem::getOrCreatePlanetChunkRenderData(const PlanetData& source)
	{
		for(PlanetChunkRenderData& planet : chunk_render_data)
		{
			if(planet.world_body_id == source.world_body_id
				&& planet.planet_generation_id == source.generation_id) return planet;
		}

		for(size_t index = 0; index < chunk_render_data.size();)
		{
			if(chunk_render_data[index].world_body_id != source.world_body_id)
			{
				index++;
				continue;
			}
			destroyPlanetLodMeshes(chunk_render_data[index]);
			for(size_t shift = index + 1; shift < chunk_render_data.size(); shift++)
				chunk_render_data[shift - 1] = static_cast<PlanetChunkRenderData&&>(chunk_render_data[shift]);
			chunk_render_data.resize(chunk_render_data.size() - 1);
		}

		PlanetChunkRenderData planet;
		planet.world_body_id = source.world_body_id;
		planet.planet_generation_id = source.generation_id;
		planet.terrain_revision = source.terrain_revision;
		planet.levels.resize(planet_chunk_level_count);
		for(uint8 level_index = 1; level_index < planet_chunk_level_count; level_index++)
		{
			PlanetChunkLevel& level = planet.levels[level_index];
			level.resolution = lod_resolutions[level_index - 1];
			level.camera_local = level_index == local_patch_level;
			level.patches.resize(level.camera_local ? 1 : global_face_patch_count);
		}
		chunk_render_data.pushMove(planet);
		return chunk_render_data[chunk_render_data.size() - 1];
	}
	/** @copydoc PlanetSystem::updateLod */
	void PlanetSystem::updateLod()
	{
		DSE_WATCH();
		for(size_t index = 0; index < chunk_render_data.size();)
		{
			if(isPlanet(chunk_render_data[index].world_body_id)
				&& world.world_body_data.isWorldBodyActive(chunk_render_data[index].world_body_id))
			{
				index++;
				continue;
			}
			DynamicArray<ChunkMeshGenerationRequest> no_requests;
			world.chunk_mesh_generation.replacePlanetLodRequests(
				chunk_render_data[index].world_body_id, chunk_render_data[index].planet_generation_id, no_requests);
			destroyPlanetLodMeshes(chunk_render_data[index]);
			for(size_t shift = index + 1; shift < chunk_render_data.size(); shift++)
				chunk_render_data[shift - 1] = static_cast<PlanetChunkRenderData&&>(chunk_render_data[shift]);
			chunk_render_data.resize(chunk_render_data.size() - 1);
		}

		for(const PlanetData& source : planets)
		{
			if(!world.world_body_data.isWorldBodyActive(source.world_body_id)) continue;
			PlanetChunkRenderData& planet = getOrCreatePlanetChunkRenderData(source);
			PlanetChunkLevel& regular_level = planet.levels[0];
			regular_level.chunk_ids.clear();
			const WorldBody& body = world.world_body_data.getWorldBody(source.world_body_id);
			for(ChunkId chunk_id : body.chunk_ids) regular_level.chunk_ids.push(chunk_id);
			if(planet.terrain_revision != source.terrain_revision)
			{
				destroyPlanetLodMeshes(planet);
				planet.terrain_revision = source.terrain_revision;
			}
			if(planet.terrain_snapshot == nullptr)
				planet.terrain_snapshot = createLodSnapshot(source);

			DynamicArray<ChunkMeshGenerationRequest> requests;
			if(!world.camera.isActive())
			{
				planet.enabled = false;
				planet.show_local_patch = false;
				planet.render_level = 0;
			}
			else
			{
				const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
					* (world.camera.getWorldPosition() - body.position);
				const double height = getPlanetHeightAboveSurface(
					source.world_body_id, local_camera_position);
				const bool was_enabled = planet.enabled;
				if(planet.enabled)
				{
					if(height < lod_deactivation_height) planet.enabled = false;
				}
				else if(height > lod_activation_height) planet.enabled = true;
				if(planet.enabled != was_enabled)
				{
					if(planet.enabled)
						danikk_framework::formatLogInfo(
							"Planet LOD enabled: body=% height_above_surface=% m activation_threshold=% m",
							source.world_body_id, height, lod_activation_height);
					else
						danikk_framework::formatLogInfo(
							"Planet LOD disabled: body=% height_above_surface=% m deactivation_threshold=% m reason=below surface coverage range",
							source.world_body_id, height, lod_deactivation_height);
				}

				const auto queue_local_patch_request = [&]()
				{
					PlanetChunkLevel& level = planet.levels[local_patch_level];
					PlanetLodPatch& patch = level.patches[local_patch_face];
					const double patch_grid_step = local_surface_patch_width * 0.5;
					const glm::dvec3 camera_direction = glm::normalize(local_camera_position);
					const glm::dvec3 camera_surface_position = camera_direction
						* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, camera_direction);
					const glm::ivec3 patch_coordinate = glm::ivec3(glm::round(
						camera_surface_position / patch_grid_step));
					const glm::dvec3 snapped_position = glm::dvec3(patch_coordinate) * patch_grid_step;
					const glm::dvec3 snapped_direction = glm::length(snapped_position) > 1e-9
						? glm::normalize(snapped_position) : camera_direction;
					const glm::dvec3 patch_origin = snapped_direction
						* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, snapped_direction);
					if(!patch.ready || patch.grid_coordinate != patch_coordinate)
					{
						if(patch.grid_coordinate != patch_coordinate)
						{
							patch.positions.clear();
							patch.triangle_indices.clear();
							patch.triangle_material_ids.clear();
							patch.ready = false;
							patch.grid_coordinate = patch_coordinate;
						}
						patch.origin = patch_origin;
						ChunkMeshGenerationRequest request;
						request.source = ChunkMeshSource::planet_surface_lod;
						request.source_revision = source.terrain_revision;
						request.world_body_id = source.world_body_id;
						request.planet_generation_id = source.generation_id;
						request.terrain_revision = source.terrain_revision;
						request.lod_resolution = level.resolution;
						request.lod_face = local_patch_face;
						request.lod_level = local_patch_level;
						request.chunk_width = local_surface_patch_width;
						request.voxel_size = local_surface_patch_width / level.resolution;
						request.grid_coordinate = patch_coordinate;
						request.chunk_position = patch_origin;
						request.planet_terrain = planet.terrain_snapshot;
						requests.pushMove(request);
					}
				};
				if(planet.enabled)
				{
					double thresholds[planet_lod_level_count - 1];
					for(uint8 threshold_index = 0; threshold_index < planet_lod_level_count - 1; threshold_index++)
					{
						const double minimum = source.chunk_load_radius * 4.0
							* static_cast<double>(threshold_index + 1);
						const double scaled = source.radius * level_altitude_ratios[threshold_index];
						thresholds[threshold_index] = glm::max(minimum, scaled);
						if(threshold_index > 0)
							thresholds[threshold_index] = glm::max(
								thresholds[threshold_index], thresholds[threshold_index - 1] * 1.5);
					}

					const bool had_desired_level = planet.has_desired_level;
					const uint8 previous_desired_level = planet.desired_level;
					if(!planet.has_desired_level)
					{
						planet.desired_level = 0;
						while(planet.desired_level < planet_lod_level_count - 1
							&& height >= thresholds[planet.desired_level]) planet.desired_level++;
						planet.has_desired_level = true;
					}
					else
					{
						while(planet.desired_level < planet_lod_level_count - 1
							&& height >= thresholds[planet.desired_level] * 1.1)
							planet.desired_level++;
						while(planet.desired_level > 0
							&& height < thresholds[planet.desired_level - 1] * 0.9)
							planet.desired_level--;
					}
					if(!had_desired_level || previous_desired_level != planet.desired_level)
						danikk_framework::formatLogInfo(
							"Planet LOD desired level changed: body=% level=% height_above_surface=% m",
							source.world_body_id, planet.desired_level, height);

					planet.show_local_patch = planet.desired_level == 0;
					if(planet.show_local_patch) queue_local_patch_request();

					const auto queue_level_requests = [&](uint8 level_index)
					{
						const uint8 render_level = static_cast<uint8>(level_index + 1);
						PlanetChunkLevel& level = planet.levels[render_level];
						if(level.camera_local)
						{
							PlanetLodPatch& patch = level.patches[local_patch_face];
							const double patch_width = glm::max(source.chunk_load_radius * 4.0, 128.0);
							const double patch_grid_step = patch_width * 0.5;
							const glm::dvec3 camera_direction = glm::normalize(local_camera_position);
							const glm::dvec3 camera_surface_position = camera_direction
								* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, camera_direction);
							const glm::ivec3 patch_coordinate = glm::ivec3(glm::round(
								camera_surface_position / patch_grid_step));
							const glm::dvec3 snapped_position = glm::dvec3(patch_coordinate) * patch_grid_step;
							const glm::dvec3 snapped_direction = glm::length(snapped_position) > 1e-9
								? glm::normalize(snapped_position) : camera_direction;
							const glm::dvec3 patch_origin = snapped_direction
								* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, snapped_direction);
							if(!patch.ready || patch.grid_coordinate != patch_coordinate)
							{
								if(patch.grid_coordinate != patch_coordinate)
								{
									patch.positions.clear();
									patch.triangle_indices.clear();
									patch.triangle_material_ids.clear();
									patch.ready = false;
									patch.grid_coordinate = patch_coordinate;
								}
								patch.origin = patch_origin;
								ChunkMeshGenerationRequest request;
								request.source = ChunkMeshSource::planet_surface_lod;
								request.source_revision = source.terrain_revision;
								request.world_body_id = source.world_body_id;
								request.planet_generation_id = source.generation_id;
								request.terrain_revision = source.terrain_revision;
								request.lod_resolution = level.resolution;
								request.lod_face = local_patch_face;
								request.lod_level = render_level;
								request.chunk_width = patch_width;
								request.voxel_size = patch_width / level.resolution;
								request.grid_coordinate = patch_coordinate;
								request.chunk_position = patch_origin;
								request.planet_terrain = planet.terrain_snapshot;
								requests.pushMove(request);
							}
							return;
						}

						for(uint8 face = 0; face < global_face_patch_count; face++)
						{
							PlanetLodPatch& patch = level.patches[face];
							if(patch.ready) continue;
							ChunkMeshGenerationRequest request;
							request.source = ChunkMeshSource::planet_surface_lod;
							request.source_revision = source.terrain_revision;
							request.world_body_id = source.world_body_id;
							request.planet_generation_id = source.generation_id;
							request.terrain_revision = source.terrain_revision;
							request.lod_resolution = level.resolution;
							request.lod_face = face;
							request.lod_level = render_level;
							request.grid_coordinate = glm::ivec3(face, render_level, 0);
							request.voxel_size = 1.57079632679489661923
								* source.radius / level.resolution;
							request.planet_terrain = planet.terrain_snapshot;
							requests.pushMove(request);
						}
					};

					if(planet.desired_level != 0)
						queue_level_requests(planet.desired_level);
					if(planet.desired_level == 0)
						queue_level_requests(1);

					const uint8 previous_render_level = planet.render_level;
					planet.render_level = 0;
					if(planet.desired_level == 0
						&& isLevelReady(planet.levels[local_patch_level]))
					{
						planet.render_level = local_patch_level;
						planet.active_level = local_patch_level;
						planet.has_active_level = true;
					}
					else
					{
						uint8 selected_level = planet_chunk_level_count;
						if(planet.desired_level > 0
							&& isLevelReady(planet.levels[planet.desired_level + 1]))
							selected_level = static_cast<uint8>(planet.desired_level + 1);
						else
						{
							for(uint8 candidate = 1; candidate < planet_lod_level_count; candidate++)
							{
								const uint8 candidate_level = static_cast<uint8>(candidate + 1);
								if(!isLevelReady(planet.levels[candidate_level])) continue;
								selected_level = candidate_level;
								break;
							}
						}
						if(selected_level < planet_chunk_level_count)
					{
						planet.render_level = selected_level;
						planet.active_level = selected_level;
						planet.has_active_level = true;
					}
					}
					if(planet.render_level != previous_render_level)
						danikk_framework::formatLogInfo(
							"Planet surface render level changed: body=% level=% height_above_surface=% m",
							source.world_body_id,
							planet.render_level, height);
				}
				else
				{
					planet.render_level = 0;
					planet.show_local_patch = false;
				}
			}
			world.chunk_mesh_generation.replacePlanetLodRequests(
				source.world_body_id, source.generation_id, requests);
		}
	}


}
