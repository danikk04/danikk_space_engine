#pragma once

#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine::mesh_generation_internal
{
	GeneratedMesh generatePlanetTerrainNodeMesh(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generatePlanetTerrainNodeMarchingTetrahedra(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generatePlanetTerrainNodeSurfaceNets(World& world, const MeshGenerationRequest& request);
	GeneratedMesh generateBuildPartMesh(World& world, const MeshGenerationRequest& request);

}
