#include <cmath>
#include <danikk_space_engine/component/world_body/planet/chunk_cache.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

namespace danikk_space_engine
{
	/*
	 * Generates a triangulated patch on a cube-sphere face. The patch coordinates are projected onto
	 * the planet sphere, displaced by the scalar terrain field, and returned in chunk-local space.
	 * Cache serialization is delegated to the planet chunk cache module.
	 */
	GeneratedChunkData generatePlanetChunk(const ChunkGenerationRequest& request)
	{
		GeneratedChunkData result;
		result.generation_id = request.generation_id;
		result.world_body_id = request.world_body_id;
		result.component_slot_id = request.component_slot_id;
		result.component_generation_id = request.component_generation_id;
		result.grid_coordinate = request.grid_coordinate;
		result.chunk_position = request.chunk_position;
		if(loadPlanetChunkCache(request, result)) return result;

		constexpr double cell_size = 4.0;
		const size_t cells_x = static_cast<size_t>(std::ceil(request.width / cell_size));
		const size_t cells_y = cells_x;
		const double step_x = request.width / static_cast<double>(cells_x);
		const double step_y = request.width / static_cast<double>(cells_y);
		const size_t points_per_row = cells_x + 1;
		result.point_positions.reserve(points_per_row * (cells_y + 1));
		result.triangle_point_indices.reserve(cells_x * cells_y * 6);

		for(size_t y_index = 0; y_index <= cells_y; y_index++)
		{
			for(size_t x_index = 0; x_index <= cells_x; x_index++)
			{
				const double x = request.x0 + static_cast<double>(x_index) * step_x;
				const double y = request.y0 + static_cast<double>(y_index) * step_y;
				const glm::dvec3 direction = glm::normalize(
					request.up * request.planet_radius
						+ request.east * x
						+ request.north * y);
				const glm::dvec3 sample_position = direction * request.planet_radius;
				const double terrain_height = samplePlanetScalarField(
					sample_position, request.planet_radius, request.noise_seed);
				const glm::dvec3 surface_position = direction * (request.planet_radius + terrain_height);
				result.point_positions.push(vec3(surface_position - request.chunk_position));
			}
		}

		for(size_t y_index = 0; y_index < cells_y; y_index++)
		{
			for(size_t x_index = 0; x_index < cells_x; x_index++)
			{
				const uint32 p00 = static_cast<uint32>(y_index * points_per_row + x_index);
				const uint32 p10 = p00 + 1;
				const uint32 p01 = p00 + static_cast<uint32>(points_per_row);
				const uint32 p11 = p01 + 1;
				result.triangle_point_indices.push(p00);
				result.triangle_point_indices.push(p10);
				result.triangle_point_indices.push(p11);
				result.triangle_point_indices.push(p00);
				result.triangle_point_indices.push(p11);
				result.triangle_point_indices.push(p01);
			}
		}
		savePlanetChunkCache(request, result);
		return result;
	}
}
