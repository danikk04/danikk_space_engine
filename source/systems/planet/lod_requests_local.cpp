#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::queuePlanetLocalLodRequests(PlanetData& planet, uint8 level_index,
		const glm::dvec3& local_camera_position,
		DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		DSE_WATCH();
		constexpr uint16 tile_radius = planet_local_heightmap_tile_radius;
		constexpr uint16 tile_side = planet_local_heightmap_tile_side;
		const double tile_widths[4] = {
			0.0, planet_chunk_width * 8.0,
			planet_chunk_width * 64.0, planet_chunk_width * 1024.0};
		PlanetChunkLevel& level = planet.levels[level_index];
		const double tile_width = tile_widths[level_index];
		const glm::dvec3 camera_direction = glm::length(local_camera_position) > 1e-9
			? glm::normalize(local_camera_position) : glm::dvec3(0.0, 0.0, 1.0);
		const double surface_radius = getPlanetSurfaceRadius(planet.world_body_id,
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
			* getPlanetSurfaceRadius(planet.world_body_id, frame_up,
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
				for(PlanetLodPatch& patch : level.patches)
				{
					patch.positions.clear();
					patch.triangle_indices.clear();
					patch.triangle_material_ids.clear();
					patch.ready = false;
				}
		}

		const auto wrap_coordinate = [](int32 coordinate)
		{
			const int32 wrapped = coordinate % tile_side;
			return wrapped < 0 ? wrapped + tile_side : wrapped;
		};
		for(int32 y_offset = -tile_radius; y_offset <= tile_radius; y_offset++)
		{
			for(int32 x_offset = -tile_radius; x_offset <= tile_radius; x_offset++)
			{
				const glm::ivec3 coordinate = center + glm::ivec3(x_offset, y_offset, 0);
				const uint16 patch_index = level_index == 1
					? static_cast<uint16>(wrap_coordinate(coordinate.y) * tile_side
						+ wrap_coordinate(coordinate.x))
					: static_cast<uint16>((y_offset + tile_radius) * tile_side
						+ x_offset + tile_radius);
				PlanetLodPatch& patch = level.patches[patch_index];
				if(patch.grid_coordinate != coordinate)
				{
					patch.positions.clear();
					patch.triangle_indices.clear();
					patch.triangle_material_ids.clear();
					patch.ready = false;
					level.initialized = false;
				}
				patch.grid_coordinate = coordinate;
				if(patch.ready) continue;

				patch.origin = level.heightmap_plane_origin
					+ level.heightmap_right * (static_cast<double>(coordinate.x) * tile_width)
					+ level.heightmap_forward * (static_cast<double>(coordinate.y) * tile_width);
				ChunkMeshGenerationRequest request;
				setPlanetLodRequestIdentity(request, planet, level, level_index, patch_index);
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
	}
}
