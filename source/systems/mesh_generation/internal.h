#pragma once

#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine::mesh_generation_internal
{
	void generatePlanetTerrainNodeMesh(World& world, MeshGenerationJob& job);
	void generatePlanetTerrainNodeMarchingTetrahedra(World& world, MeshGenerationJob& job);
	void generatePlanetTerrainNodeSurfaceNets(World& world, MeshGenerationJob& job);
	void generateBuildPartMesh(World& world, MeshGenerationJob& job);

}
