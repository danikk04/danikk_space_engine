#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/assets/collection.h>

#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 coarse_chunk_resolution = 16;
		constexpr uint32 local_heightmap_resolution = 64;
		constexpr uint32 global_heightmap_resolutions[2] = {29, 9};
		constexpr uint16 local_tile_radius = planet_local_heightmap_tile_radius;
		constexpr uint16 local_tile_side = planet_local_heightmap_tile_side;
		constexpr uint32 local_tile_count = planet_local_heightmap_tile_count;
		constexpr double lod_activation_height = 100.0;
		constexpr double lod_deactivation_height = 70.0;
		constexpr uint8 first_local_heightmap_level = 1;
		constexpr uint8 last_local_heightmap_level = 3;
		constexpr uint8 first_global_heightmap_level = 4;
		constexpr uint8 last_lod_level = planet_chunk_level_count - 1;

		bool isLevelReady(const PlanetChunkLevel& level)
		{
			if(!level.initialized || level.mesh == nullptr || level.expected_patch_count == 0
				|| level.patches.size() != level.expected_patch_count) return false;
			for(const PlanetLodPatch& patch : level.patches)
				if(!patch.ready) return false;
			return true;
		}

		void clearPatchGeometry(PlanetLodPatch& patch)
		{
			patch.positions.clear();
			patch.triangle_indices.clear();
			patch.triangle_material_ids.clear();
			patch.ready = false;
		}

		void setRequestIdentity(ChunkMeshGenerationRequest& request, const PlanetData& planet,
			const PlanetChunkLevel& level,
			uint8 level_index, uint16 patch_index)
		{
			request.source = ChunkMeshSource::planet_surface_lod;
			request.source_revision = planet.terrain_revision;
			request.world_body_id = planet.world_body_id;
			request.planet_generation_id = planet.generation_id;
			request.terrain_revision = planet.terrain_revision;
			request.lod_resolution = level.resolution;
			request.lod_face = patch_index;
			request.lod_level = level_index;
			request.planet_mesh_generator = planet.last_chunk_update_mesh_generator;
		}
	}

	void PlanetSystem::initializePlanetLodLevels(PlanetData& planet)
	{
		if(planet.levels.size() != 0) return;
		planet.levels.resize(planet_chunk_level_count);
		planet.levels[0].geometry = PlanetChunkLevelGeometry::regular_volume_chunks;
		for(uint8 level_index = 1; level_index < planet_chunk_level_count; level_index++)
		{
			PlanetChunkLevel& level = planet.levels[level_index];
			if(level_index == 1)
			{
				level.geometry = PlanetChunkLevelGeometry::local_heightmap;
				level.resolution = coarse_chunk_resolution;
				level.expected_patch_count = local_tile_count;
				level.camera_local = true;
				level.patches.resize(local_tile_count);
			}
			else if(level_index <= last_local_heightmap_level)
			{
				level.geometry = PlanetChunkLevelGeometry::local_heightmap;
				level.resolution = local_heightmap_resolution;
				level.expected_patch_count = local_tile_count;
				level.camera_local = true;
				level.patches.resize(local_tile_count);
			}
			else
			{
				level.geometry = PlanetChunkLevelGeometry::global_heightmap;
				level.resolution = global_heightmap_resolutions[level_index - first_global_heightmap_level];
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
		for(PlanetData& source : planets)
		{
			if(!world.world_body_data.isWorldBodyActive(source.world_body_id))
			{
				DynamicArray<ChunkMeshGenerationRequest> no_requests;
				world.chunk_mesh_generation.replacePlanetLodRequests(
					source.world_body_id, source.generation_id, no_requests);
				destroyPlanetLodMeshes(source);
				source.lod_enabled = false;
				continue;
			}
			initializePlanetLodLevels(source);
			const PlanetSettings& source_settings = getPlanetSettings(source.settings_id);
			PlanetChunkLevel& regular_level = source.levels[0];
			regular_level.chunk_ids.clear();
			const WorldBody& body = world.world_body_data.getWorldBody(source.world_body_id);
			for(ChunkId chunk_id : body.chunk_ids) regular_level.chunk_ids.push(chunk_id);
			if(source.lod_terrain_revision != source.terrain_revision)
			{
				destroyPlanetLodMeshes(source);
				source.lod_terrain_revision = source.terrain_revision;
			}
			DynamicArray<ChunkMeshGenerationRequest> requests;
			if(!world.camera.isActive())
			{
				source.lod_enabled = false;
				source.lod_level = 0;
			}
			else
			{
				const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
					* (world.camera.getWorldPosition() - body.position);
				const double height = getPlanetHeightAboveSurface(
					source.world_body_id, local_camera_position);
				const bool was_enabled = source.lod_enabled;
				if(source.lod_enabled)
				{
					if(height < lod_deactivation_height) source.lod_enabled = false;
				}
				else if(height > lod_activation_height) source.lod_enabled = true;
				if(source.lod_enabled != was_enabled)
				{
					if(source.lod_enabled)
						danikk_framework::formatLogInfo(
							"Planet LOD enabled: body=% height_above_surface=% m activation_threshold=% m",
							source.world_body_id, height, lod_activation_height);
					else
						danikk_framework::formatLogInfo(
							"Planet LOD disabled: body=% height_above_surface=% m deactivation_threshold=% m reason=below surface coverage range",
							source.world_body_id, height, lod_deactivation_height);
				}

				if(source.lod_enabled)
				{
					const double level_widths[planet_chunk_level_count] = {
						0.0, planet_chunk_width * 8.0,
						planet_chunk_width * 64.0, planet_chunk_width * 1024.0,
						0.0, 0.0};
					const double thresholds[planet_chunk_level_count - 2] = {
						glm::max(source_settings.chunk_load_radius * 4.0, level_widths[1] * 2.5),
						glm::max(source_settings.radius * 0.003, level_widths[2] * 2.0),
						glm::max(source_settings.radius * 0.05, level_widths[3] * 2.0),
						source_settings.radius * 0.75};

					const uint8 previous_lod_level = source.lod_level;
					uint8 target_level = source.lod_level;
					if(target_level == 0)
					{
						target_level = 1;
						while(target_level < last_lod_level
							&& height >= thresholds[target_level - 1])
							target_level++;
					}
					else
					{
						while(target_level < last_lod_level
							&& height >= thresholds[target_level - 1] * 1.1)
							target_level++;
						while(target_level > 1
							&& height < thresholds[target_level - 2] * 0.9)
							target_level--;
					}
					if(target_level != previous_lod_level)
						danikk_framework::formatLogInfo(
							"Planet LOD target level changed: body=% level=% height_above_surface=% m",
							source.world_body_id, target_level, height);

					const auto queue_level_requests = [&](uint8 level_index)
					{
						PlanetChunkLevel& level = source.levels[level_index];
						if(level_index >= first_local_heightmap_level
							&& level_index <= last_local_heightmap_level)
						{
							const double tile_width = level_widths[level_index];
							const glm::dvec3 camera_direction = glm::length(local_camera_position) > 1e-9
								? glm::normalize(local_camera_position) : glm::dvec3(0.0, 0.0, 1.0);
							const double surface_radius = getPlanetSurfaceRadius(source.world_body_id,
								camera_direction, PlanetTerrainMode::heightmap_with_edits);
							const glm::dvec3 camera_surface_position = camera_direction * surface_radius;
							const int32 face_axis = glm::abs(camera_direction.x) >= glm::abs(camera_direction.y)
								&& glm::abs(camera_direction.x) >= glm::abs(camera_direction.z)
								? (camera_direction.x >= 0.0 ? 0 : 1)
								: glm::abs(camera_direction.y) >= glm::abs(camera_direction.z)
									? (camera_direction.y >= 0.0 ? 2 : 3)
									: (camera_direction.z >= 0.0 ? 4 : 5);
							glm::dvec3 frame_up(0.0);
							glm::dvec3 frame_right(0.0);
							glm::dvec3 frame_forward(0.0);
							switch(face_axis)
							{
								case 0:
									frame_up = glm::dvec3(1, 0, 0);
									frame_right = glm::dvec3(0, 1, 0);
									frame_forward = glm::dvec3(0, 0, 1);
									break;
								case 1:
									frame_up = glm::dvec3(-1, 0, 0);
									frame_right = glm::dvec3(0, -1, 0);
									frame_forward = glm::dvec3(0, 0, 1);
									break;
								case 2:
									frame_up = glm::dvec3(0, 1, 0);
									frame_right = glm::dvec3(-1, 0, 0);
									frame_forward = glm::dvec3(0, 0, 1);
									break;
								case 3:
									frame_up = glm::dvec3(0, -1, 0);
									frame_right = glm::dvec3(1, 0, 0);
									frame_forward = glm::dvec3(0, 0, 1);
									break;
								case 4:
									frame_up = glm::dvec3(0, 0, 1);
									frame_right = glm::dvec3(1, 0, 0);
									frame_forward = glm::dvec3(0, 1, 0);
									break;
								default:
									frame_up = glm::dvec3(0, 0, -1);
									frame_right = glm::dvec3(-1, 0, 0);
									frame_forward = glm::dvec3(0, 1, 0);
									break;
							}
							const glm::dvec3 frame_plane_origin = frame_up
								* getPlanetSurfaceRadius(source.world_body_id, frame_up,
									PlanetTerrainMode::heightmap_with_edits);
							const int32 center_x = static_cast<int32>(std::floor(
								glm::dot(camera_surface_position - frame_plane_origin, frame_right) / tile_width + 0.5));
							const int32 center_y = static_cast<int32>(std::floor(
								glm::dot(camera_surface_position - frame_plane_origin, frame_forward) / tile_width + 0.5));
							const glm::ivec3 center(center_x, center_y, face_axis);
							if(!level.grid_center_set || level.grid_center != center)
							{
								level.grid_center = center;
								level.grid_center_set = true;
								level.heightmap_right = frame_right;
								level.heightmap_forward = frame_forward;
								level.heightmap_plane_origin = frame_plane_origin;
								level.mesh_origin = frame_plane_origin
									+ frame_right * (static_cast<double>(center_x) * tile_width)
									+ frame_forward * (static_cast<double>(center_y) * tile_width);
											level.initialized = false;
											if(level_index != 1)
												for(PlanetLodPatch& patch : level.patches) clearPatchGeometry(patch);
							}
							for(int32 y_offset = -local_tile_radius; y_offset <= local_tile_radius; y_offset++)
							{
								for(int32 x_offset = -local_tile_radius; x_offset <= local_tile_radius; x_offset++)
								{
								const auto wrap_tile_coordinate = [](int32 coordinate)
								{
								const int32 wrapped = coordinate % local_tile_side;
								return wrapped < 0 ? wrapped + local_tile_side : wrapped;
								};
								const glm::ivec3 coordinate = center
									+ glm::ivec3(x_offset, y_offset, 0);
								const uint16 patch_index = level_index == 1
									? static_cast<uint16>(wrap_tile_coordinate(coordinate.y) * local_tile_side
										+ wrap_tile_coordinate(coordinate.x))
									: static_cast<uint16>((y_offset + local_tile_radius) * local_tile_side
										+ x_offset + local_tile_radius);
									PlanetLodPatch& patch = level.patches[patch_index];
								if(patch.grid_coordinate != coordinate)
								{
									clearPatchGeometry(patch);
									level.initialized = false;
								}
									patch.grid_coordinate = coordinate;
									if(patch.ready) continue;
									patch.origin = level.heightmap_plane_origin
										+ level.heightmap_right * (static_cast<double>(coordinate.x) * tile_width)
										+ level.heightmap_forward * (static_cast<double>(coordinate.y) * tile_width);
									ChunkMeshGenerationRequest request;
									setRequestIdentity(request, source, level, level_index, patch_index);
									request.chunk_width = tile_width;
									request.voxel_size = tile_width / level.resolution;
									request.grid_coordinate = coordinate;
									request.chunk_position = patch.origin;
									request.heightmap_right = level.heightmap_right;
									request.heightmap_forward = level.heightmap_forward;
									request.has_heightmap_basis = true;
									requests.pushMove(request);
								}
							}
							return;
						}

						for(uint8 face = 0; face < planet_lod_face_count; face++)
						{
							PlanetLodPatch& patch = level.patches[face];
							patch.grid_coordinate = glm::ivec3(face, level_index, 0);
							if(patch.ready) continue;
							ChunkMeshGenerationRequest request;
							setRequestIdentity(request, source, level, level_index, face);
							request.grid_coordinate = glm::ivec3(face, level_index, 0);
							request.voxel_size = 1.57079632679489661923
								* source_settings.radius / level.resolution;
							requests.pushMove(request);
						}
					};

					queue_level_requests(target_level);

					uint8 selected_level = 0;
					if(isLevelReady(source.levels[target_level]))
						selected_level = target_level;
					else
					{
						if(previous_lod_level > 0 && previous_lod_level < source.levels.size()
							&& source.levels[previous_lod_level].mesh != nullptr)
							selected_level = previous_lod_level;
						else if(source.levels[target_level].mesh != nullptr)
							selected_level = target_level;
						else if(target_level != first_global_heightmap_level
							&& isLevelReady(source.levels[first_global_heightmap_level]))
							selected_level = first_global_heightmap_level;
					}
					source.lod_level = selected_level;
					if(source.lod_level != previous_lod_level)
						danikk_framework::formatLogInfo(
							"Planet surface render level changed: body=% level=% height_above_surface=% m",
							source.world_body_id, source.lod_level, height);
				}
				else
				{
					source.lod_level = 0;
				}
			}
			world.chunk_mesh_generation.replacePlanetLodRequests(
				source.world_body_id, source.generation_id, requests);
		}
	}


}
