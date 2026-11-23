#pragma once

#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>

namespace danikk_space_engine::mesh_generation_internal
{
	uint8 getPlanetTerrainNodeLevel(double node_width);
	bool sameJobKey(const MeshGenerationJob& a, const MeshGenerationJob& b);
	bool hasMatchingJob(const MeshGenerationJob& candidate,
		const Array<MeshGenerationJob, 8>& jobs);
	void generateMesh(World& world, MeshGenerationJob& job);
	void generatePlanetNodeGeometry(World& world, MeshGenerationJob& job);
	void generatePlanetTerrainNodeMesh(World& world, MeshGenerationJob& job);
	void generatePlanetTerrainNodeMarchingTetrahedra(World& world, MeshGenerationJob& job);
	void generatePlanetTerrainNodeSurfaceNets(World& world, MeshGenerationJob& job);
	void generateBuildPartMesh(World& world, MeshGenerationJob& job);

}
