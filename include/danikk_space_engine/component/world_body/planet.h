#pragma once

#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/world/planet/chunk_generator.h>

namespace danikk_space_engine
{
	inline constexpr double default_planet_radius = 500.0;
	inline constexpr double planet_chunk_stream_radius = 100.0;

	struct PlanetComponentSettings
	{
		double radius = default_planet_radius;
		double chunk_stream_radius = planet_chunk_stream_radius;
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
		double chunk_stream_radius = planet_chunk_stream_radius;
		uint64 noise_seed = 0;
		glm::dvec3 stream_anchor = glm::dvec3(0.0);
		bool stream_initialized = false;
		bool is_streaming = false;
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
