#pragma once

#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{
	using ChunkId = uint32;
	using WorldBodyId = uint32;

	struct ChunkGenerationRequest
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		WorldBodyComponentSlotId component_slot_id = 0;
		uint64 component_generation_id = 0;
		double planet_radius = 0.0;
		uint64 noise_seed = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		glm::dvec3 up = glm::dvec3(0.0, 0.0, 1.0);
		glm::dvec3 east = glm::dvec3(1.0, 0.0, 0.0);
		glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0);
		double x0 = 0.0;
		double y0 = 0.0;
		double width = 0.0;
	};

	struct GeneratedChunkData
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		WorldBodyComponentSlotId component_slot_id = 0;
		uint64 component_generation_id = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		DynamicArray<vec3> point_positions;
		DynamicArray<uint32> triangle_point_indices;
	};

	GeneratedChunkData generatePlanetChunk(const ChunkGenerationRequest& request);
}
