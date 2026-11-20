#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/planet/system.h>

namespace danikk_space_engine
{
	void PlanetSystem::queuePlanetGlobalLodRequests(PlanetData& planet, uint8 level_index,
		const PlanetSettings& settings, DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		DSE_WATCH();
		PlanetChunkLevel& level = planet.levels[level_index];
		for(uint8 face = 0; face < planet_lod_face_count; face++)
		{
			PlanetLodPatch& patch = level.patches[face];
			patch.grid_coordinate = glm::ivec3(face, level_index, 0);
			if(patch.ready) continue;

			ChunkMeshGenerationRequest request;
			setPlanetLodRequestIdentity(request, planet, level, level_index, face);
			request.grid_coordinate = glm::ivec3(face, level_index, 0);
			request.voxel_size = 1.57079632679489661923 * settings.radius / level.resolution;
			requests.pushMove(request);
		}
	}
}
