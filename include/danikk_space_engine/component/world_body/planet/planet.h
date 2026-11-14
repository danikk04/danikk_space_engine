#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	struct FacePtr;

	inline constexpr double default_planet_radius = 5000.0;
	inline constexpr double planet_chunk_load_radius = 100.0;
	inline constexpr double planet_chunk_width = 32.0;
	inline constexpr double planet_voxel_size = 4.0;

	struct PlanetComponentSettings
	{
		double radius = default_planet_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		uint64 noise_seed = 0;
	};

	struct PlanetChunkRecord
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = static_cast<ChunkId>(-1);
	};

	struct PlanetChunkLookupEntry
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = static_cast<ChunkId>(-1);
		uint8 state = 0;
		bool desired = false;
		bool has_record = false;
	};

	struct PlanetChunkLookup
	{
		DynamicArray<PlanetChunkLookupEntry> entries;
		size_t entry_count = 0;
		size_t used_bucket_count = 0;

		void clearDesired();
		void markDesired(const glm::ivec3& grid_coordinate);
		void setChunk(const glm::ivec3& grid_coordinate, ChunkId chunk_id);
		void remove(const glm::ivec3& grid_coordinate);
		bool isDesired(const glm::ivec3& grid_coordinate) const;
		bool hasChunk(const glm::ivec3& grid_coordinate) const;
	};

	struct PlanetComponent
	{
		double radius = default_planet_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		uint64 noise_seed = 0;
		glm::dvec3 last_chunk_update_position = glm::dvec3(0.0);
		uint64 terrain_revision = 0;
		uint64 last_chunk_update_revision = 0;
		size_t processed_edit_count = 0;
		bool terrain_edits_loaded = false;
		bool chunk_set_initialized = false;
		bool chunk_loading_active = false;
		DynamicArray<PlanetTerrainEdit> terrain_edits;
		DynamicArray<PlanetChunkRecord> chunks;
		PlanetChunkLookup chunk_lookup;
		DynamicArray<ChunkGenerationRequest> pending_requests;
	};

	void constructPlanetComponent(void* destination, const void* init_data);
	void movePlanetComponent(void* destination, void* source);
	void destroyPlanetComponent(void* data);
	void framePlanetComponent(WorldBodyComponentSlot& slot, World& world);
	void acceptPlanetGeneratedChunk(
		WorldBodyComponentSlot& slot,
		World& world,
		const GeneratedChunkData& generated_chunk);
	bool editPlanetGround(World& world, const FacePtr& face, double radius, bool add_material);
}
