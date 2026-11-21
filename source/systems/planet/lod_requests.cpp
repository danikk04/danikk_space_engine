#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::setPlanetLodRequestIdentity(ChunkMeshGenerationRequest& request,
		const PlanetData& planet, const PlanetChunkLevel& level,
		uint8 level_index, uint16 patch_index)
	{
		DSE_WATCH();
		request.source = ChunkMeshSource::planet_surface_lod;
		request.source_revision = planet.terrain_revision;
		request.world_body_id = planet.world_body_id;
		request.planet_generation_id = planet.generation_id;
		request.terrain_revision = planet.terrain_revision;
		request.lod_resolution = level.resolution;
		request.lod_face = patch_index;
		request.lod_level = level_index;
		request.planet_mesh_generator = planet.last_chunk_update_mesh_generator;
	}

	void PlanetSystem::queuePlanetLodRequests(PlanetData& planet, uint8 level_index,
		const glm::dvec3& local_camera_position,
		const PlanetSettings& settings, DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		DSE_WATCH();
		const PlanetLodCoverage coverage = getPlanetLodSettings(level_index).coverage;
		if(coverage == PlanetLodCoverage::fixed_diameter
			|| coverage == PlanetLodCoverage::horizon)
			queuePlanetLocalLodRequests(planet, level_index, local_camera_position, requests);
		else
			queuePlanetGlobalLodRequests(planet, level_index, settings, requests);
	}
}
