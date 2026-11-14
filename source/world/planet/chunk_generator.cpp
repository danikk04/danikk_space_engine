#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 chunk_cache_magic = 0x4443484Bu;
		constexpr uint32 chunk_cache_version = 5;
		constexpr bool chunk_cache_enabled = false;

		struct ChunkCacheHeader
		{
			uint32 magic;
			uint32 version;
			uint32 face;
			int32 x;
			int32 y;
			uint32 point_count;
			uint32 index_count;
			double planet_radius;
			uint64 noise_seed;
		};

		std::filesystem::path cacheFilePath(const ChunkGenerationRequest& request)
		{
			return std::filesystem::current_path() / "cache" / "planet_surface"
				/ ("v5_" + std::to_string(request.world_body_id) + "_" + std::to_string(request.noise_seed)
					+ "_" + std::to_string(request.grid_coordinate.z)
					+ "_" + std::to_string(request.grid_coordinate.x)
					+ "_" + std::to_string(request.grid_coordinate.y) + ".bin");
		}

		bool loadCachedChunk(const ChunkGenerationRequest& request, GeneratedChunkData& result)
		{
			std::ifstream input(cacheFilePath(request), std::ios::binary);
			if(!input) return false;
			ChunkCacheHeader header{};
			input.read(reinterpret_cast<char*>(&header), sizeof(header));
			if(!input || header.magic != chunk_cache_magic || header.version != chunk_cache_version
				|| header.face != static_cast<uint32>(request.grid_coordinate.z)
				|| header.x != request.grid_coordinate.x || header.y != request.grid_coordinate.y
				|| header.planet_radius != request.planet_radius || header.noise_seed != request.noise_seed
				|| header.point_count > 100000 || header.index_count > 1000000 || header.index_count % 3 != 0)
			{
				return false;
			}

			DynamicArray<vec3> point_positions;
			point_positions.resize(header.point_count);
			for(vec3& position : point_positions)
			{
				input.read(reinterpret_cast<char*>(&position.x), sizeof(float));
				input.read(reinterpret_cast<char*>(&position.y), sizeof(float));
				input.read(reinterpret_cast<char*>(&position.z), sizeof(float));
			}
			DynamicArray<uint32> triangle_point_indices;
			triangle_point_indices.resize(header.index_count);
			input.read(reinterpret_cast<char*>(triangle_point_indices.data()),
				static_cast<std::streamsize>(header.index_count * sizeof(uint32)));
			if(!input) return false;
			for(uint32 point_index : triangle_point_indices)
			{
				if(point_index >= point_positions.size()) return false;
			}
			result.point_positions = static_cast<DynamicArray<vec3>&&>(point_positions);
			result.triangle_point_indices = static_cast<DynamicArray<uint32>&&>(triangle_point_indices);
			return true;
		}

		void saveCachedChunk(const ChunkGenerationRequest& request, const GeneratedChunkData& result)
		{
			std::error_code error;
			const std::filesystem::path file_path = cacheFilePath(request);
			std::filesystem::create_directories(file_path.parent_path(), error);
			if(error) return;
			std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
			if(!output) return;
			const ChunkCacheHeader header{
				chunk_cache_magic, chunk_cache_version,
				static_cast<uint32>(request.grid_coordinate.z), request.grid_coordinate.x, request.grid_coordinate.y,
				static_cast<uint32>(result.point_positions.size()),
				static_cast<uint32>(result.triangle_point_indices.size()),
				request.planet_radius, request.noise_seed};
			output.write(reinterpret_cast<const char*>(&header), sizeof(header));
			for(const vec3& position : result.point_positions)
			{
				output.write(reinterpret_cast<const char*>(&position.x), sizeof(float));
				output.write(reinterpret_cast<const char*>(&position.y), sizeof(float));
				output.write(reinterpret_cast<const char*>(&position.z), sizeof(float));
			}
			output.write(reinterpret_cast<const char*>(result.triangle_point_indices.data()),
				static_cast<std::streamsize>(result.triangle_point_indices.size() * sizeof(uint32)));
		}
	}

	/*
	 * Generates a triangulated patch on a cube-sphere face. The patch coordinates are projected onto
	 * the planet sphere, displaced by the scalar terrain field, and returned in chunk-local space.
	 * Optional cache serialization stays here with the geometry algorithm; ChunkLoader only schedules work.
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
		if(chunk_cache_enabled && loadCachedChunk(request, result)) return result;

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
		if(chunk_cache_enabled) saveCachedChunk(request, result);
		return result;
	}
}
