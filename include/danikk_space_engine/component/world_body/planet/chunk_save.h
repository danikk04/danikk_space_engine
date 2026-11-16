#pragma once

#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{

	/**
	 * @brief Loads planet chunk.
	 */
	bool loadPlanetChunk(const ChunkGenerationRequest& request, GeneratedChunkData& result);

	/**
	 * @brief Saves planet chunk.
	 */
	void savePlanetChunk(const ChunkGenerationRequest& request, const GeneratedChunkData& result);

	/**
	 * @brief Loads planet terrain edits.
	 */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		DynamicArray<PlanetTerrainEdit>& edits,
		uint64& terrain_revision);

	/**
	 * @brief Saves planet terrain edits.
	 */
	void savePlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		uint64 terrain_revision);
}
