#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	inline constexpr double default_planet_radius = 500.0;
	inline constexpr double planet_chunk_load_radius = 100.0;

	struct PlanetComponentSettings
	{
		double radius = default_planet_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		uint64 noise_seed = 0;
	};

	struct PlanetChunkRecord
	{
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		ChunkId chunk_id = 0;
	};

	struct PlanetComponent
	{
		double radius = default_planet_radius;
		double chunk_load_radius = planet_chunk_load_radius;
		uint64 noise_seed = 0;
		glm::dvec3 last_chunk_update_direction = glm::dvec3(0.0);
		bool chunk_set_initialized = false;
		bool chunk_loading_active = false;
		DynamicArray<PlanetChunkRecord> chunks;
		DynamicArray<glm::ivec3> desired_chunk_keys;
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
}
