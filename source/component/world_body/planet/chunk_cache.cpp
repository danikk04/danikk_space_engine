#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

#include <danikk_space_engine/component/world_body/planet/chunk_cache.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 chunk_cache_magic = 0x4443484Bu;
		constexpr uint32 chunk_cache_version = 6;
		constexpr uint32 terrain_edits_magic = 0x44504544u;
		constexpr uint32 terrain_edits_version = 1;

		struct ChunkCacheHeader
		{
			uint32 magic;
			uint32 version;
			int32 x;
			int32 y;
			int32 z;
			uint32 point_count;
			uint32 index_count;
			double planet_radius;
			double chunk_width;
			double voxel_size;
			uint64 noise_seed;
			uint64 terrain_revision;
		};

		struct TerrainEditsHeader
		{
			uint32 magic;
			uint32 version;
			double planet_radius;
			uint64 noise_seed;
			uint64 terrain_revision;
			uint32 edit_count;
		};

		std::filesystem::path cacheFilePath(const ChunkGenerationRequest& request)
		{
			return std::filesystem::current_path() / "cache" / "planet_surface"
				/ ("v6_" + std::to_string(request.world_body_id) + "_" + std::to_string(request.noise_seed)
					+ "_" + std::to_string(request.terrain_revision)
					+ "_" + std::to_string(request.grid_coordinate.x)
					+ "_" + std::to_string(request.grid_coordinate.y)
					+ "_" + std::to_string(request.grid_coordinate.z) + ".bin");
		}

		std::filesystem::path terrainEditsFilePath(WorldBodyId world_body_id, uint64 noise_seed)
		{
			return std::filesystem::current_path() / "cache" / "planet_surface"
				/ ("edits_" + std::to_string(world_body_id) + "_" + std::to_string(noise_seed) + ".bin");
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
			|| header.x != request.grid_coordinate.x || header.y != request.grid_coordinate.y
			|| header.z != request.grid_coordinate.z
			|| header.planet_radius != request.planet_radius || header.noise_seed != request.noise_seed
			|| header.chunk_width != request.chunk_width || header.voxel_size != request.voxel_size
			|| header.terrain_revision != request.terrain_revision
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
		result.terrain_revision = request.terrain_revision;
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
			request.grid_coordinate.x, request.grid_coordinate.y, request.grid_coordinate.z,
			static_cast<uint32>(result.point_positions.size()),
			static_cast<uint32>(result.triangle_point_indices.size()),
			request.planet_radius, request.chunk_width, request.voxel_size,
			request.noise_seed, request.terrain_revision};
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

	/*
	 * Loads the persistent list of spherical terrain operations for one planet identity. The file header
	 * validates the radius, seed, format version, and count before edit records are accepted; malformed
	 * or truncated data is ignored so the planet can still start from its procedural base field.
	 */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		DynamicArray<PlanetTerrainEdit>& edits,
		uint64& terrain_revision)
	{
		std::ifstream input(terrainEditsFilePath(world_body_id, noise_seed), std::ios::binary);
		if(!input) return false;
		TerrainEditsHeader header{};
		input.read(reinterpret_cast<char*>(&header), sizeof(header));
		if(!input || header.magic != terrain_edits_magic || header.version != terrain_edits_version
			|| header.planet_radius != planet_radius || header.noise_seed != noise_seed
			|| header.edit_count > 1000000) return false;

		DynamicArray<PlanetTerrainEdit> loaded_edits;
		loaded_edits.resize(header.edit_count);
		for(PlanetTerrainEdit& edit : loaded_edits)
		{
			uint8 add_material = 0;
			input.read(reinterpret_cast<char*>(&edit.center.x), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.center.y), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.center.z), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.radius), sizeof(double));
			input.read(reinterpret_cast<char*>(&add_material), sizeof(add_material));
			if(!input || !std::isfinite(edit.radius) || edit.radius <= 0.0 || add_material > 1) return false;
			edit.add_material = add_material != 0;
		}
		edits = static_cast<DynamicArray<PlanetTerrainEdit>&&>(loaded_edits);
		terrain_revision = header.terrain_revision;
		return true;
	}

	/*
	 * Persists terrain edits independently from generated meshes. Each operation is serialized field by
	 * field rather than as a padded struct, and a failed disk write never interrupts terrain generation.
	 */
	void savePlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		uint64 terrain_revision)
	{
		std::error_code error;
		const std::filesystem::path file_path = terrainEditsFilePath(world_body_id, noise_seed);
		std::filesystem::create_directories(file_path.parent_path(), error);
		if(error) return;
		std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
		if(!output) return;
		const TerrainEditsHeader header{
			terrain_edits_magic, terrain_edits_version, planet_radius, noise_seed,
			terrain_revision, static_cast<uint32>(edits.size())};
		output.write(reinterpret_cast<const char*>(&header), sizeof(header));
		for(const PlanetTerrainEdit& edit : edits)
		{
			const uint8 add_material = edit.add_material ? 1 : 0;
			output.write(reinterpret_cast<const char*>(&edit.center.x), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.center.y), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.center.z), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.radius), sizeof(double));
			output.write(reinterpret_cast<const char*>(&add_material), sizeof(add_material));
		}
	}
}
