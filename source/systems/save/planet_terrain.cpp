#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

#include <danikk_framework/path_buffer.h>
#include <danikk_framework/log.h>
#include <danikk_space_engine/systems/save/system.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr bool enable_terrain_edit_save_logs = true;
		constexpr uint32 terrain_edits_magic = 0x44504544u;
		constexpr uint32 terrain_edits_version = 2;

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

		/** @brief Returns the terrain-edit path for one planet and procedural seed. */
		std::filesystem::path terrainEditsFilePath(PlanetId planet_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "planets" / std::to_string(planet_id) / "terrain"
				/ ("edits_" + std::to_string(noise_seed) + ".bin");
		}

		/** @brief Returns the terrain-edit path used before saves were split by world body. */
		std::filesystem::path legacyTerrainEditsFilePath(PlanetId planet_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "terrain"
				/ ("edits_" + std::to_string(planet_id) + "_" + std::to_string(noise_seed) + ".bin");
		}

		/** @brief Returns the previous per-WorldBody path so existing worlds keep their edits. */
		std::filesystem::path worldBodyTerrainEditsFilePath(PlanetId previous_body_id, uint64 noise_seed)
		{
			return saveRootDirectory() / "world_bodies" / std::to_string(previous_body_id) / "terrain"
				/ ("edits_" + std::to_string(noise_seed) + ".bin");
		}
	}

	/** @brief Loads and validates a planet's ordered terrain stamps and revision counter. */
	bool SaveSystem::loadPlanetTerrainEdits(PlanetId planet_id)
	{
		PlanetData* planet_data = world.getPlanetSystem().findPlanet(planet_id);
		if(planet_data == nullptr) return false;
		PlanetData& planet = *planet_data;
		if(planet.terrain_edits_loaded) return true;
		const PlanetSettings& settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
		const double planet_radius = settings.radius;
		const uint64 noise_seed = settings.noise_seed;
		const std::filesystem::path new_file_path = terrainEditsFilePath(planet_id, noise_seed);
		const std::filesystem::path legacy_file_path = legacyTerrainEditsFilePath(planet_id, noise_seed);
		const std::filesystem::path previous_file_path = worldBodyTerrainEditsFilePath(planet_id, noise_seed);
		std::ifstream input(new_file_path, std::ios::binary);
		bool loaded_legacy_file = false;
		if(!input)
		{
			input.clear();
			input.open(legacy_file_path, std::ios::binary);
			loaded_legacy_file = static_cast<bool>(input);
		}
		if(!input)
		{
			input.clear();
			input.open(previous_file_path, std::ios::binary);
			loaded_legacy_file = static_cast<bool>(input);
		}
		if(!input)
		{
			planet.terrain_edits_loaded = true;
			if constexpr(enable_terrain_edit_save_logs)
				formatLogInfo("Planet terrain edits not found: planet=%", planet_id);
			return false;
		}

		TerrainEditsHeader header{};
		input.read(reinterpret_cast<char*>(&header), sizeof(header));
		if(!input || header.magic != terrain_edits_magic
			|| (header.version != 1 && header.version != terrain_edits_version)
			|| header.planet_radius != planet_radius || header.noise_seed != noise_seed
			|| header.edit_count > 1000000)
		{
			planet.terrain_edits_loaded = true;
			if constexpr(enable_terrain_edit_save_logs)
				formatLogWarning(
					"Planet terrain edits rejected: planet=% read_ok=% magic=% version=% count=% radius_match=% seed_match=%",
					planet_id, static_cast<bool>(input), header.magic, header.version,
					header.edit_count, header.planet_radius == planet_radius,
					header.noise_seed == noise_seed);
			return false;
		}

		DynamicArray<PlanetTerrainEdit> loaded_edits;
		loaded_edits.reserve(header.edit_count);
		for(uint32 index = 0; index < header.edit_count; index++)
		{
			PlanetTerrainEdit edit;
			if(!edit.read(input, header.version))
			{
				planet.terrain_edits_loaded = true;
				if constexpr(enable_terrain_edit_save_logs)
					formatLogWarning("Planet terrain edit record rejected: planet=% record=% of %",
						planet_id, index, header.edit_count);
				return false;
			}
			loaded_edits.push(edit);
		}
		for(const PlanetTerrainEdit& edit : loaded_edits)
			if(planet.terrain_edits.append(edit) == static_cast<PlanetTerrainEditId>(-1))
				formatLogWarning("Ignoring terrain edit that exceeds the spatial index range");
		planet.terrain_revision = header.terrain_revision;
		if(loaded_legacy_file || header.version != terrain_edits_version)
		{
			input.close();
			savePlanetTerrainEdits(planet_id);
			std::error_code error;
			if(loaded_legacy_file && std::filesystem::exists(new_file_path, error) && !error)
			{
				if(std::filesystem::exists(legacy_file_path, error) && !error)
					std::filesystem::remove(legacy_file_path, error);
				if(std::filesystem::exists(previous_file_path, error) && !error)
					std::filesystem::remove(previous_file_path, error);
			}
		}
		planet.terrain_edits_loaded = true;
		if constexpr(enable_terrain_edit_save_logs)
			formatLogInfo("Planet terrain edits loaded: planet=% count=% revision=%",
				planet_id, planet.terrain_edits.ids.size(), planet.terrain_revision);
		return true;
	}

	/** @brief Writes terrain-edit stamps field by field and logs path or write failures. */
	void SaveSystem::savePlanetTerrainEdits(PlanetId planet_id)
	{
		const PlanetData* planet_data = world.getPlanetSystem().findPlanet(planet_id);
		if(planet_data == nullptr) return;
		const PlanetData& planet = *planet_data;
		std::error_code error;
		const PlanetSettings& settings = world.getPlanetSystem().getPlanetSettings(planet.settings_id);
		const double planet_radius = settings.radius;
		const uint64 noise_seed = settings.noise_seed;
		const std::filesystem::path file_path = terrainEditsFilePath(planet_id, noise_seed);
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
			planet.terrain_revision, static_cast<uint32>(planet.terrain_edits.pool->usedIds().size())};
		output.write(reinterpret_cast<const char*>(&header), sizeof(header));
		for(PlanetTerrainEditId edit_id : planet.terrain_edits.ids)
			if(planet.terrain_edits.pool->isActive(edit_id))
				(*planet.terrain_edits.pool)[edit_id].write(output);
		output.close();
		if(!output) formatLogError("Unable to write terrain edits save: %", file_path.string().c_str());
		else if constexpr(enable_terrain_edit_save_logs)
			formatLogInfo("Planet terrain edits saved: planet=% count=% revision=% path=%",
				planet_id, planet.terrain_edits.ids.size(), planet.terrain_revision,
				file_path.string().c_str());
	}
}
