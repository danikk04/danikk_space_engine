#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	namespace planet_settings_ids
	{
		inline constexpr PlanetSettingsId earth = 0;
		inline constexpr PlanetSettingsId count = 1;
	}

	inline constexpr double default_planet_radius = 6371000.0;
	inline constexpr double default_planet_core_radius = 3480000.0;
	inline constexpr double planet_chunk_load_radius = 100.0;
	inline constexpr double planet_chunk_width = 32.0;
	inline constexpr double planet_voxel_size = 4.0;

	/**
	 * @brief Stores a named planet preset including radii, gravity, and terrain-generation seed.
	 */
	struct PlanetComponentSettings
	{
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		double surface_gravity = 9.8;
		uint64 noise_seed = 0;
	};

	/**
	 * @brief Maps a planet grid coordinate to its chunk ID and desired/generated state.
	 */
	struct PlanetChunkLookupEntry
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		uint8 state = 0;
		bool desired = false;
		bool has_record = false;
	};

	/**
	 * @brief Maps planet grid coordinates to generated chunk records.
	 */
	struct PlanetChunkLookup
	{
		DynamicArray<PlanetChunkLookupEntry> entries;
		size_t entry_count = 0;
		size_t used_bucket_count = 0;

		/**
		 * @brief Clears desired flags and tombstones entries that no longer own loaded chunks.
		 */
		void clearDesired();

		/**
		 * @brief Marks this coordinate as needed in the current streaming update.
		 */
		void markDesired(const glm::ivec3& grid_coordinate);

		/**
		 * @brief Sets chunk.
		 */
		void setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id);

		/**
		 * @brief Removes a loaded coordinate from the lookup table and clears its chunk record.
		 */
		void remove(const glm::ivec3& grid_coordinate);

		/**
		 * @brief Reports whether the grid coordinate is in the current desired chunk set.
		 */
		bool isDesired(const glm::ivec3& grid_coordinate) const;
		// True when a generation record exists, including a known-empty coordinate with no WorldData chunk.

		/**
		 * @brief Reports whether generation has produced a chunk for the grid coordinate.
		 */
		bool isGenerated(const glm::ivec3& grid_coordinate) const;

		/**
		 * @brief Returns a checked reference to an active chunk; an invalid ID raises a fatal error.
		 */
		bool getChunk(const glm::ivec3& grid_coordinate, ChunkId& chunk_id) const;
	};

	/**
	 * @brief Stores planet settings and the state of its procedurally generated chunks.
	 */
	struct PlanetComponent
	{
		const char* name = "Earth";
		double radius = default_planet_radius;
		double core_radius = default_planet_core_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		double surface_gravity = 9.8;
		uint64 noise_seed = 0;
		glm::dvec3 last_chunk_update_position = glm::dvec3(0.0);
		uint64 terrain_revision = 0;
		uint64 last_chunk_update_revision = 0;
		size_t processed_edit_count = 0;
		bool terrain_edits_loaded = false;
		bool chunk_set_initialized = false;
		bool chunk_updates_active = false;
		DynamicArray<PlanetTerrainEdit> terrain_edits;
		PlanetChunkLookup chunk_lookup;
		DynamicArray<ChunkGenerationRequest> pending_requests;
	};

	/**
	 * @brief Returns planet settings.
	 */
	const PlanetComponentSettings& getPlanetSettings(PlanetSettingsId id);

	/**
	 * @brief Constructs planet component.
	 */
	void constructPlanetComponent(void* destination, const void* init_data);

	/**
	 * @brief Moves planet component.
	 */
	void movePlanetComponent(void* destination, void* source);

	/**
	 * @brief Removes and releases planet component.
	 */
	void destroyPlanetComponent(void* data);

	/**
	 * @brief Updates planet component.
	 */
	void updatePlanetComponent(WorldBodyComponentSlot& slot, World& world);

	/**
	 * @brief Accepts planet generated chunk.
	 */
	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk);

	/**
	 * @brief Records a terrain stamp for the selected planet face and invalidates affected chunks.
	 */
	bool editPlanetGround(World& world, const FacePtr& face, double radius, bool adds_solid_terrain);
}
