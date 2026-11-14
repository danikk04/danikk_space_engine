#pragma once

#include <danikk_space_engine/component/world_body/planet/chunk_generator.h>

namespace danikk_space_engine
{
	bool loadPlanetChunkCache(const ChunkGenerationRequest& request, GeneratedChunkData& result);
	void savePlanetChunkCache(const ChunkGenerationRequest& request, const GeneratedChunkData& result);
}
