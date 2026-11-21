#pragma once

#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>

namespace danikk_space_engine::chunk_mesh_generation_internal
{
	GeneratedChunkMesh generatePlanetTerrainNodeMesh(World& world, const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generatePlanetTerrainChunkMeshSurfaceNets(World& world, const ChunkMeshGenerationRequest& request);
	GeneratedChunkMesh generateBuildPartChunkMesh(World& world, const ChunkMeshGenerationRequest& request);
}
