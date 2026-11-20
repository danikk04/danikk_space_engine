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
		constexpr uint8 local_tile_radius = planet_local_heightmap_tile_radius;
		constexpr uint8 local_tile_side = planet_local_heightmap_tile_side;
		constexpr uint32 local_tile_count = planet_local_heightmap_tile_count;
		constexpr double lod_activation_height = 100.0;
		constexpr double lod_deactivation_height = 70.0;
		constexpr uint8 first_local_heightmap_level = 2;
		constexpr uint8 last_local_heightmap_level = 3;
		constexpr uint8 first_global_heightmap_level = 4;
		constexpr uint8 last_lod_level = planet_chunk_level_count - 1;

		std::shared_ptr<const PlanetTerrainSnapshot> createLodSnapshot(const PlanetData& planet)
		{
			const PlanetSettings& settings = getPlanetSettings(planet.settings_id);
			std::shared_ptr<PlanetTerrainSnapshot> snapshot = std::make_shared<PlanetTerrainSnapshot>();
			snapshot->world_body_id = planet.world_body_id;
			snapshot->planet_generation_id = planet.generation_id;
			snapshot->terrain_revision = planet.terrain_revision;
			snapshot->planet_radius = settings.radius;
			snapshot->core_radius = settings.core_radius;
			snapshot->noise_seed = settings.noise_seed;
			snapshot->terrain_edits = planet.terrain_edits;
			return snapshot;
		}

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
			const PlanetChunkRenderData& render_data, const PlanetChunkLevel& level,
			uint8 level_index, uint8 patch_index)
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
			request.planet_terrain = render_data.terrain_snapshot;
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
		planet.levels[0].geometry = PlanetChunkLevelGeometry::regular_volume_chunks;
		for(uint8 level_index = 1; level_index < planet_chunk_level_count; level_index++)
		{
			PlanetChunkLevel& level = planet.levels[level_index];
			if(level_index == 1)
			{
				level.geometry = PlanetChunkLevelGeometry::coarse_volume_chunks;
				level.resolution = coarse_chunk_resolution;
				level.expected_patch_count = planet_coarse_chunk_count;
				level.camera_local = true;
				level.patches.resize(planet_coarse_chunk_count);
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
			const PlanetSettings& source_settings = getPlanetSettings(source.settings_id);
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
				planet.lod_level = 0;
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

				if(planet.enabled)
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

					const uint8 previous_lod_level = planet.lod_level;
					uint8 target_level = planet.lod_level;
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
						PlanetChunkLevel& level = planet.levels[level_index];
						if(level_index == 1)
						{
							const double chunk_width = planet_chunk_width * 8.0;
							const glm::ivec3 center = glm::ivec3(glm::floor(
								local_camera_position / chunk_width));
							if(!level.grid_center_set || level.grid_center != center)
							{
								level.grid_center = center;
								level.grid_center_set = true;
								level.mesh_origin = (glm::dvec3(center) + glm::dvec3(0.5)) * chunk_width;
								level.initialized = false;
								for(PlanetLodPatch& patch : level.patches) clearPatchGeometry(patch);
							}
							for(int32 z = -planet_coarse_chunk_half_width; z <= planet_coarse_chunk_half_width; z++)
							{
								for(int32 y = -planet_coarse_chunk_half_width; y <= planet_coarse_chunk_half_width; y++)
								{
									for(int32 x = -planet_coarse_chunk_half_width; x <= planet_coarse_chunk_half_width; x++)
									{
										const uint8 patch_index = static_cast<uint8>(
											((z + planet_coarse_chunk_half_width) * planet_coarse_chunk_side
											+ (y + planet_coarse_chunk_half_width)) * planet_coarse_chunk_side
											+ x + planet_coarse_chunk_half_width);
										PlanetLodPatch& patch = level.patches[patch_index];
										const glm::ivec3 coordinate = center + glm::ivec3(x, y, z);
										if(patch.grid_coordinate != coordinate) clearPatchGeometry(patch);
										patch.grid_coordinate = coordinate;
										if(patch.ready) continue;
										patch.origin = glm::dvec3(coordinate) * chunk_width;
										ChunkMeshGenerationRequest request;
										setRequestIdentity(request, source, planet, level, level_index, patch_index);
										request.chunk_width = chunk_width;
										request.voxel_size = planet_voxel_size * 8.0;
										request.grid_coordinate = coordinate;
										request.chunk_position = patch.origin;
										requests.pushMove(request);
									}
								}
							}
							return;
						}

						if(level_index >= first_local_heightmap_level
							&& level_index <= last_local_heightmap_level)
						{
							const double tile_width = level_widths[level_index];
							const glm::dvec3 camera_direction = glm::length(local_camera_position) > 1e-9
								? glm::normalize(local_camera_position) : glm::dvec3(0.0, 0.0, 1.0);
							const double surface_radius = PlanetSystem::getPlanetSurfaceRadius(
								*planet.terrain_snapshot, camera_direction);
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
								* PlanetSystem::getPlanetSurfaceRadius(*planet.terrain_snapshot, frame_up);
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
								for(PlanetLodPatch& patch : level.patches) clearPatchGeometry(patch);
							}
							for(int32 y_offset = -local_tile_radius; y_offset <= local_tile_radius; y_offset++)
							{
								for(int32 x_offset = -local_tile_radius; x_offset <= local_tile_radius; x_offset++)
								{
									const uint8 patch_index = static_cast<uint8>(
										(y_offset + local_tile_radius) * local_tile_side
										+ x_offset + local_tile_radius);
									PlanetLodPatch& patch = level.patches[patch_index];
									const glm::ivec3 coordinate = center
										+ glm::ivec3(x_offset, y_offset, 0);
									if(patch.grid_coordinate != coordinate) clearPatchGeometry(patch);
									patch.grid_coordinate = coordinate;
									if(patch.ready) continue;
									patch.origin = level.heightmap_plane_origin
										+ level.heightmap_right * (static_cast<double>(coordinate.x) * tile_width)
										+ level.heightmap_forward * (static_cast<double>(coordinate.y) * tile_width);
									ChunkMeshGenerationRequest request;
									setRequestIdentity(request, source, planet, level, level_index, patch_index);
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
							setRequestIdentity(request, source, planet, level, level_index, face);
							request.grid_coordinate = glm::ivec3(face, level_index, 0);
							request.voxel_size = 1.57079632679489661923
								* source_settings.radius / level.resolution;
							requests.pushMove(request);
						}
					};

					queue_level_requests(target_level);

					uint8 selected_level = 0;
					if(isLevelReady(planet.levels[target_level]))
						selected_level = target_level;
					else
					{
						if(target_level != first_global_heightmap_level
							&& isLevelReady(planet.levels[first_global_heightmap_level]))
							selected_level = first_global_heightmap_level;
					}
					planet.lod_level = selected_level;
					if(planet.lod_level != previous_lod_level)
						danikk_framework::formatLogInfo(
							"Planet surface render level changed: body=% level=% height_above_surface=% m",
							source.world_body_id, planet.lod_level, height);
				}
				else
				{
					planet.lod_level = 0;
				}
			}
			world.chunk_mesh_generation.replacePlanetLodRequests(
				source.world_body_id, source.generation_id, requests);
		}
	}


}
