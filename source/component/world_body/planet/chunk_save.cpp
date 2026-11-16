#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

#include <danikk_framework/path_buffer.h>
#include <danikk_framework/log.h>
#include <danikk_space_engine/component/world_body/planet/chunk_save.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 chunk_save_magic = 0x4443484Bu;
		constexpr uint32 chunk_save_version = 9;
		constexpr uint32 terrain_edits_magic = 0x44504544u;
		constexpr uint32 terrain_edits_version = 1;

		/**
		 * @brief Stores the signature and version used to validate a saved terrain chunk.
		 */
		struct ChunkSaveHeader
		{
			uint32 magic;
			uint32 version;
			int32 x;
			int32 y;
			int32 z;
			uint32 point_count;
			uint32 index_count;
			double planet_radius;
			double core_radius;
			double chunk_width;
			double voxel_size;
			uint64 noise_seed;
			uint64 terrain_revision;
		};

		/**
		 * @brief Stores the signature and version used to validate a saved terrain-edit file.
		 */
		struct TerrainEditsHeader
		{
			uint32 magic;
			uint32 version;
			double planet_radius;
			uint64 noise_seed;
			uint64 terrain_revision;
			uint32 edit_count;
		};

		/**
		 * @brief Saves root directory.
		 */
		std::filesystem::path saveRootDirectory()
		{
			return std::filesystem::path(danikk_framework::getExecutableDirectory().c_string())
				/ "saves" / "default";
		}

		/**
		 * @brief Returns the save directory unique to the specified world body.
		 */
		std::filesystem::path worldBodyTerrainDirectory(WorldBodyId world_body_id)
		{
			return saveRootDirectory() / "world_bodies" / std::to_string(world_body_id) / "terrain";
		}

		/**
		 * @brief Formats the chunk grid coordinate as its save filename.
		 */
		std::filesystem::path chunkSaveFileName(
			const ChunkGenerationRequest& request,
			bool include_version,
			uint32 version,
			bool include_body_id)
		{
			const std::string body_id_prefix = include_body_id
				? std::to_string(request.world_body_id) + "_" : "";
			const std::string version_prefix = include_version ? "v" + std::to_string(version) + "_" : "";
			return version_prefix + body_id_prefix + std::to_string(request.noise_seed)
				+ "_" + std::to_string(request.terrain_revision)
				+ "_" + std::to_string(request.grid_coordinate.x)
				+ "_" + std::to_string(request.grid_coordinate.y)
				+ "_" + std::to_string(request.grid_coordinate.z) + ".bin";
		}

		/**
		 * @brief Returns the chunk file path beneath the owning world body’s terrain directory.
		 */
		std::filesystem::path chunkSaveFilePath(const ChunkGenerationRequest& request)
		{
			return worldBodyTerrainDirectory(request.world_body_id) / "chunks"
				/ chunkSaveFileName(request, false, chunk_save_version, false);
		}

		/**
		 * @brief Returns the current-version chunk file path for the request coordinate.
		 */
		std::filesystem::path versionedChunkSaveFilePath(const ChunkGenerationRequest& request)
		{
			return worldBodyTerrainDirectory(request.world_body_id) / "chunks"
				/ chunkSaveFileName(request, true, chunk_save_version, false);
		}

		/**
		 * @brief Returns the legacy chunk file path used before per-body terrain saves.
		 */
		std::filesystem::path legacyChunkSaveFilePath(const ChunkGenerationRequest& request)
		{
			return saveRootDirectory() / "terrain"
				/ chunkSaveFileName(request, true, 6, true);
		}

		/**
		 * @brief Returns the current terrain-edit file path for the body and noise seed.
		 */
		std::filesystem::path terrainEditsFilePath(WorldBodyId world_body_id, uint64 noise_seed)
		{
			return worldBodyTerrainDirectory(world_body_id)
				/ ("edits_" + std::to_string(noise_seed) + ".bin");
		}

		/**
		 * @brief Returns the terrain-edit path used by the older save layout.
		 */
		std::filesystem::path legacyTerrainEditsFilePath(WorldBodyId world_body_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "terrain"
				/ ("edits_" + std::to_string(world_body_id) + "_" + std::to_string(noise_seed) + ".bin");
		}
	}

	/**
	 * @brief Validates the saved chunk header and arrays before replacing the worker result.
	 *
	 * Geometry is read into temporary arrays. A corrupt or truncated file leaves
	 * the caller's result unchanged so generation can safely fall back to rebuilding.
	 * @param request Generation parameters used to locate and verify the save.
	 * @param result Output object updated only after the file passes all integrity checks.
	 * @return True for a valid chunk file, false for missing, incompatible, or malformed data.
	 */
	bool loadPlanetChunk(const ChunkGenerationRequest& request, GeneratedChunkData& result)
	{
		const std::filesystem::path new_file_path = chunkSaveFilePath(request);
		const std::filesystem::path versioned_file_path = versionedChunkSaveFilePath(request);
		const std::filesystem::path legacy_file_path = legacyChunkSaveFilePath(request);
		std::ifstream input(new_file_path, std::ios::binary);
		std::filesystem::path loaded_file_path = new_file_path;
		if(!input)
		{
			input.clear();
			input.open(versioned_file_path, std::ios::binary);
			if(input) loaded_file_path = versioned_file_path;
		}
		if(!input)
		{
			input.clear();
			input.open(legacy_file_path, std::ios::binary);
			if(input) loaded_file_path = legacy_file_path;
		}
		if(!input) return false;
		ChunkSaveHeader header{};
		input.read(reinterpret_cast<char*>(&header), sizeof(header));
		if(!input || header.magic != chunk_save_magic || header.version != chunk_save_version
			|| header.x != request.grid_coordinate.x || header.y != request.grid_coordinate.y
			|| header.z != request.grid_coordinate.z
			|| header.planet_radius != request.planet_radius || header.core_radius != request.core_radius
			|| header.noise_seed != request.noise_seed
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
		result.generation_id = request.generation_id;
		result.world_body_id = request.world_body_id;
		result.component_slot_id = request.component_slot_id;
		result.component_generation_id = request.component_generation_id;
		result.terrain_revision = request.terrain_revision;
		result.grid_coordinate = request.grid_coordinate;
		result.chunk_position = request.chunk_position;
		if(loaded_file_path != new_file_path)
		{
			input.close();
			savePlanetChunk(request, result);
			std::error_code error;
			if(std::filesystem::exists(new_file_path, error) && !error)
				std::filesystem::remove(loaded_file_path, error);
		}
		return true;
	}

	/**
	 * @brief Serializes chunk-local point positions and triangle indices with versioned generation metadata.
	 *
	 * Write failures are logged; generated geometry remains available to the running
	 * world. Triangle material IDs are recalculated from the scalar field when loaded.
	 * @param request Chunk key and planet parameters used for the path and file header.
	 * @param result Generated geometry to persist; material IDs are regenerated from the field.
	 */
	void savePlanetChunk(const ChunkGenerationRequest& request, const GeneratedChunkData& result)
	{
		std::error_code error;
		const std::filesystem::path file_path = chunkSaveFilePath(request);
		std::filesystem::create_directories(file_path.parent_path(), error);
		if(error)
		{
			formatLogError("Unable to create terrain save directory: %", file_path.parent_path().string().c_str());
			return;
		}
		std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
		if(!output)
		{
			formatLogError("Unable to open chunk save: %", file_path.string().c_str());
			return;
		}
		const ChunkSaveHeader header{
			chunk_save_magic, chunk_save_version,
			request.grid_coordinate.x, request.grid_coordinate.y, request.grid_coordinate.z,
			static_cast<uint32>(result.point_positions.size()),
			static_cast<uint32>(result.triangle_point_indices.size()),
			request.planet_radius, request.core_radius, request.chunk_width, request.voxel_size,
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
		output.close();
		if(!output) formatLogError("Unable to write chunk save: %", file_path.string().c_str());
	}

	/**
	 * @brief Loads and validates the body's ordered terrain stamps and their revision counter.
	 *
	 * Reads into a temporary array and replaces the outputs only after the header
	 * and every record validate. Malformed or truncated data is ignored, allowing
	 * the planet to fall back to its procedural base field. A compatible legacy
	 * file is migrated to the current per-body path.
	 * @param world_body_id Body whose save folder is searched.
	 * @param planet_radius Expected radius recorded in the header.
	 * @param noise_seed Expected procedural seed recorded in the header.
	 * @param edits Output array replaced after every stamp passes validation.
	 * @param terrain_revision Output revision paired with the edit list.
	 * @return True when a valid current or legacy file was loaded; false when no valid file exists.
	 */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		DynamicArray<PlanetTerrainEdit>& edits,
		uint64& terrain_revision)
	{
		const std::filesystem::path new_file_path = terrainEditsFilePath(world_body_id, noise_seed);
		const std::filesystem::path legacy_file_path = legacyTerrainEditsFilePath(world_body_id, noise_seed);
		std::ifstream input(new_file_path, std::ios::binary);
		bool loaded_legacy_file = false;
		if(!input)
		{
			input.clear();
			input.open(legacy_file_path, std::ios::binary);
			loaded_legacy_file = static_cast<bool>(input);
		}
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
			uint8 adds_solid_terrain = 0;
			input.read(reinterpret_cast<char*>(&edit.center.x), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.center.y), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.center.z), sizeof(double));
			input.read(reinterpret_cast<char*>(&edit.radius), sizeof(double));
			input.read(reinterpret_cast<char*>(&adds_solid_terrain), sizeof(adds_solid_terrain));
			if(!input || !std::isfinite(edit.radius) || edit.radius <= 0.0 || adds_solid_terrain > 1) return false;
			edit.adds_solid_terrain = adds_solid_terrain != 0;
		}
		edits = static_cast<DynamicArray<PlanetTerrainEdit>&&>(loaded_edits);
		terrain_revision = header.terrain_revision;
		if(loaded_legacy_file)
		{
			input.close();
			savePlanetTerrainEdits(
				world_body_id, planet_radius, noise_seed, edits, terrain_revision);
			std::error_code error;
			if(std::filesystem::exists(new_file_path, error) && !error)
				std::filesystem::remove(legacy_file_path, error);
		}
		return true;
	}

	/**
	 * @brief Serializes the edit list field-by-field and logs any directory or write failure.
	 *
	 * Edits are written as individual fields rather than raw padded structures.
	 * A failed save is logged and does not stop in-memory terrain generation.
	 * @param world_body_id Body-specific save directory key.
	 * @param planet_radius Radius written for compatibility checking on load.
	 * @param noise_seed Procedural seed written for compatibility checking on load.
	 * @param edits Ordered add/remove stamps to write.
	 * @param terrain_revision Revision associated with the list.
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
		if(error)
		{
			formatLogError("Unable to create terrain save directory: %", file_path.parent_path().string().c_str());
			return;
		}
		std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
		if(!output)
		{
			formatLogError("Unable to open terrain edits save: %", file_path.string().c_str());
			return;
		}
		const TerrainEditsHeader header{
			terrain_edits_magic, terrain_edits_version, planet_radius, noise_seed,
			terrain_revision, static_cast<uint32>(edits.size())};
		output.write(reinterpret_cast<const char*>(&header), sizeof(header));
		for(const PlanetTerrainEdit& edit : edits)
		{
			const uint8 adds_solid_terrain = edit.adds_solid_terrain ? 1 : 0;
			output.write(reinterpret_cast<const char*>(&edit.center.x), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.center.y), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.center.z), sizeof(double));
			output.write(reinterpret_cast<const char*>(&edit.radius), sizeof(double));
			output.write(reinterpret_cast<const char*>(&adds_solid_terrain), sizeof(adds_solid_terrain));
		}
		output.close();
		if(!output) formatLogError("Unable to write terrain edits save: %", file_path.string().c_str());
	}
}
