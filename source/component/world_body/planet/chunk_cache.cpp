#include <filesystem>
#include <fstream>
#include <string>

#include <danikk_space_engine/component/world_body/planet/chunk_cache.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 chunk_cache_magic = 0x4443484Bu;
		constexpr uint32 chunk_cache_version = 5;

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
	}

	/*
	 * Loads a chunk cache entry only when its version, grid key, planet parameters, array sizes, and
	 * every triangle index are valid. Geometry is first read into temporary arrays, so a corrupt or
	 * truncated file leaves the caller's result unchanged and can safely fall back to regeneration.
	 */
	bool loadPlanetChunkCache(const ChunkGenerationRequest& request, GeneratedChunkData& result)
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

	/*
	 * Writes the generated local-space geometry with its cache key and format version. Directory or
	 * file errors simply skip caching; they do not prevent the generated chunk from being returned.
	 */
	void savePlanetChunkCache(const ChunkGenerationRequest& request, const GeneratedChunkData& result)
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
