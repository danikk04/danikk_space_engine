#pragma once

#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{

	/**
	 * @brief Loads a saved mesh when its format and generation parameters match the request.
	 *
	 * Reads into temporary arrays and validates vertex/index bounds before modifying `result`.
	 * Compatible legacy paths are migrated to the current per-WorldBody save path.
	 * @param request Chunk coordinate, planet parameters, revision, and save identity to validate.
	 * @param result Receives local vertices/indices and request metadata only after complete validation.
	 * @return True when a compatible file was loaded; false when missing, stale, corrupt, or truncated.
	 */
	bool loadPlanetChunk(const ChunkGenerationRequest& request, GeneratedChunkData& result);

	/**
	 * @brief Writes generated chunk-local geometry and its compatibility header to the save directory.
	 *
	 * The versioned header records coordinate, planet dimensions, voxel spacing, noise seed, and
	 * terrain revision. I/O failures are logged and do not invalidate the in-memory generated result.
	 * @param request Request whose body ID determines the save directory and whose parameters form the cache key.
	 * @param result Generated local vertices and triangle point indices to persist.
	 */
	void savePlanetChunk(const ChunkGenerationRequest& request, const GeneratedChunkData& result);

	/**
	 * @brief Loads persistent spherical add/remove stamps for one planet body and noise seed.
	 * @param world_body_id Body-specific save identity.
	 * @param planet_radius Radius checked against the edit-file header.
	 * @param noise_seed Seed checked against the edit-file header.
	 * @param edits Receives validated edits only when the full file is valid.
	 * @param terrain_revision Receives the revision stored with the edits.
	 * @return True when a compatible edit file is loaded; false if no compatible file is available.
	 */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		DynamicArray<PlanetTerrainEdit>& edits,
		uint64& terrain_revision);

	/**
	 * @brief Persists terrain edit stamps independently of generated chunk meshes.
	 * @param world_body_id Body-specific save identity.
	 * @param planet_radius Radius written to the file header for compatibility validation.
	 * @param noise_seed Seed written to the file header for compatibility validation.
	 * @param edits Ordered spherical add/remove stamps to serialize.
	 * @param terrain_revision Revision associated with the saved edit list.
	 */
	void savePlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		uint64 terrain_revision);
}
