#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

#include <danikk_framework/path_buffer.h>
#include <danikk_framework/log.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr uint32 terrain_edits_magic = 0x44504544u;
		constexpr uint32 terrain_edits_version = 1;

		/** @brief Stores the signature and version used to validate a terrain-edit file. */
		struct TerrainEditsHeader
		{
			uint32 magic;
			uint32 version;
			double planet_radius;
			uint64 noise_seed;
			uint64 terrain_revision;
			uint32 edit_count;
		};

		/** @brief Returns the save directory for the current default game. */
		std::filesystem::path saveRootDirectory()
		{
			return std::filesystem::path(danikk_framework::getExecutableDirectory().c_string())
				/ "saves" / "default";
		}

		/** @brief Returns the terrain-edit path for one world body and procedural seed. */
		std::filesystem::path terrainEditsFilePath(WorldBodyId world_body_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "world_bodies" / std::to_string(world_body_id) / "terrain"
				/ ("edits_" + std::to_string(noise_seed) + ".bin");
		}

		/** @brief Returns the terrain-edit path used before saves were split by world body. */
		std::filesystem::path legacyTerrainEditsFilePath(WorldBodyId world_body_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "terrain"
				/ ("edits_" + std::to_string(world_body_id) + "_" + std::to_string(noise_seed) + ".bin");
		}
	}

	/** @brief Loads and validates the body's ordered terrain stamps and revision counter. */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		PlanetTerrainEditPool& edit_pool,
		DynamicArray<PlanetTerrainEditId>& edit_ids,
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

		DynamicArray<PlanetTerrainEditId> loaded_edit_ids;
		loaded_edit_ids.reserve(header.edit_count);
		for(uint32 index = 0; index < header.edit_count; index++)
		{
			PlanetTerrainEdit edit;
			if(!edit.read(input)) return false;
			const PlanetTerrainEditId edit_id = edit_pool.create();
			edit_pool[edit_id] = edit;
			loaded_edit_ids.push(edit_id);
		}
		edit_ids = static_cast<DynamicArray<PlanetTerrainEditId>&&>(loaded_edit_ids);
		terrain_revision = header.terrain_revision;
		if(loaded_legacy_file)
		{
			input.close();
			savePlanetTerrainEdits(world_body_id, planet_radius, noise_seed,
				edit_pool, edit_ids, terrain_revision);
			std::error_code error;
			if(std::filesystem::exists(new_file_path, error) && !error)
				std::filesystem::remove(legacy_file_path, error);
		}
		return true;
	}

	/** @brief Writes terrain-edit stamps field by field and logs path or write failures. */
	void savePlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		const PlanetTerrainEditPool& edit_pool,
		const DynamicArray<PlanetTerrainEditId>& edit_ids,
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
			terrain_revision, static_cast<uint32>(edit_ids.size())};
		output.write(reinterpret_cast<const char*>(&header), sizeof(header));
		for(PlanetTerrainEditId edit_id : edit_ids)
			edit_pool[edit_id].write(output);
		output.close();
		if(!output) formatLogError("Unable to write terrain edits save: %", file_path.string().c_str());
	}
}
