#pragma once

#include <danikk_space_engine/planet_system/terrain.h>
#include <danikk_space_engine/id.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	/** @brief Loads persistent terrain edit stamps for one planet body and procedural seed. */
	bool loadPlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		DynamicArray<PlanetTerrainEdit>& edits,
		uint64& terrain_revision);

	/** @brief Persists the ordered terrain edit stamps for one planet body. */
	void savePlanetTerrainEdits(
		WorldBodyId world_body_id,
		double planet_radius,
		uint64 noise_seed,
		const DynamicArray<PlanetTerrainEdit>& edits,
		uint64 terrain_revision);
}
