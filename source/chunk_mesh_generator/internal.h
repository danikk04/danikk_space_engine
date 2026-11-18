#pragma once

#include <danikk_space_engine/chunk_mesh_generator.h>

namespace danikk_space_engine::chunk_mesh_generator_internal
{
	GeneratedChunkMesh generatePlanetTerrainChunkMesh(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generateBuildPartChunkMesh(const ChunkMeshGenerationRequest& request);
}
