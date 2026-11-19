#pragma once

#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	GeneratedChunkMesh generatePlanetTerrainChunkMesh(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetTerrainChunkMeshSurfaceNets(const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generateBuildPartChunkMesh(const ChunkMeshGenerationRequest& request);
}
