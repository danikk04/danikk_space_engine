#pragma once

#include <danikk_space_engine/id.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/default.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_space_engine/component/world_body/planet/terrain.h>
#include <glm/glm.hpp>

namespace danikk_space_engine
{

	/**
	 * @brief Represents chunk generation request data and behavior.
	 */
	struct ChunkGenerationRequest
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		WorldBodyComponentSlotId component_slot_id = 0;
		uint64 component_generation_id = 0;
		uint64 terrain_revision = 0;
		double planet_radius = 0.0;
		double core_radius = 0.0;
		double chunk_width = 32.0;
		double voxel_size = 4.0;
		uint64 noise_seed = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		DynamicArray<PlanetTerrainEdit> terrain_edits;
	};

	/**
	 * @brief Represents generated chunk data data and behavior.
	 */
	struct GeneratedChunkData
	{
		uint64 generation_id = 0;
		WorldBodyId world_body_id = 0;
		WorldBodyComponentSlotId component_slot_id = 0;
		uint64 component_generation_id = 0;
		uint64 terrain_revision = 0;
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		glm::dvec3 chunk_position = glm::dvec3(0.0);
		DynamicArray<vec3> point_positions;
		DynamicArray<uint32> triangle_point_indices;
		DynamicArray<MaterialId> triangle_material_ids;
	};

	/**
	 * @brief Generates planet chunk.
	 */
	GeneratedChunkData generatePlanetChunk(const ChunkGenerationRequest& request);
}
