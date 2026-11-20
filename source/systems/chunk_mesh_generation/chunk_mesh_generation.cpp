#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_framework/log.h>

#include "internal.h"

namespace danikk_space_engine
{
	ChunkMeshGenerationSystem::ChunkMeshGenerationSystem(World& world)
		: world(world), worker(std::make_unique<chunk_mesh_generation_internal::Worker>(world)) {}

	ChunkMeshGenerationSystem::~ChunkMeshGenerationSystem() { shutdown(); }

	bool ChunkMeshGenerationSystem::getAverageGenerationTimeMilliseconds(float& average_time) const
	{
		return worker != nullptr && worker->getAverageGenerationTimeMilliseconds(average_time);
	}

	/** @copydoc ChunkMeshGenerationSystem::init */
	void ChunkMeshGenerationSystem::init()
	{
		DSE_WATCH();
		if(initialized) return;
		danikk_framework::formatLogInfo("Init ChunkMeshGenerationSystem");
		worker->start();
		initialized = true;
		generation_id++;
		if(generation_id == 0) generation_id++;
		requests.clear();
		requests_dirty = false;
		worker->update(generation_id, requests);
	}

	/** @copydoc ChunkMeshGenerationSystem::shutdown */
	void ChunkMeshGenerationSystem::shutdown()
	{
		DSE_WATCH();
		if(!initialized) return;
		danikk_framework::formatLogInfo("Shutdown ChunkMeshGenerationSystem");
		worker->stop();
		initialized = false;
		generation_id++;
		requests.clear();
		requests_dirty = false;
	}

	/** @copydoc ChunkMeshGenerationSystem::replacePlanetRequests */
	void ChunkMeshGenerationSystem::replacePlanetRequests(WorldBodyId world_body_id,
		uint64 planet_generation_id, const DynamicArray<ChunkMeshGenerationRequest>& planet_requests)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size();)
		{
			const ChunkMeshGenerationRequest& queued = requests[index];
			if(queued.source != ChunkMeshSource::planet_terrain
				|| queued.world_body_id != world_body_id
				|| queued.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
		}
		for(const ChunkMeshGenerationRequest& request : planet_requests)
		{
			ChunkMeshGenerationRequest queued = request;
			queued.generation_id = generation_id;
			requests.pushMove(queued);
		}
		requests_dirty = true;
	}

	/** @copydoc ChunkMeshGenerationSystem::replacePlanetLodRequests */
	void ChunkMeshGenerationSystem::replacePlanetLodRequests(WorldBodyId world_body_id,
		uint64 planet_generation_id, const DynamicArray<ChunkMeshGenerationRequest>& lod_requests)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size();)
		{
			const ChunkMeshGenerationRequest& queued = requests[index];
			if(queued.source != ChunkMeshSource::planet_surface_lod
				|| queued.world_body_id != world_body_id
				|| queued.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
		}
		for(const ChunkMeshGenerationRequest& request : lod_requests)
		{
			ChunkMeshGenerationRequest queued = request;
			queued.generation_id = generation_id;
			requests.pushMove(queued);
		}
		requests_dirty = true;
	}

	/** @copydoc ChunkMeshGenerationSystem::removePlanetRequest */
	void ChunkMeshGenerationSystem::removePlanetRequest(WorldBodyId world_body_id,
		uint64 planet_generation_id, const glm::ivec3& grid_coordinate)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if((request.source != ChunkMeshSource::planet_terrain
				&& request.source != ChunkMeshSource::planet_surface_lod)
				|| request.world_body_id != world_body_id
				|| request.planet_generation_id != planet_generation_id
				|| request.grid_coordinate.x != grid_coordinate.x
				|| request.grid_coordinate.y != grid_coordinate.y
				|| request.grid_coordinate.z != grid_coordinate.z) continue;
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
			return;
		}
	}

	/** @copydoc ChunkMeshGenerationSystem::cancelPlanetRequests */
	void ChunkMeshGenerationSystem::cancelPlanetRequests(WorldBodyId world_body_id,
		uint64 planet_generation_id)
	{
		DSE_WATCH();
		worker->cancel(world_body_id, planet_generation_id);
		for(size_t index = 0; index < requests.size();)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if(request.source != ChunkMeshSource::planet_terrain
				|| request.world_body_id != world_body_id
				|| request.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
		}
	}

	/** @copydoc ChunkMeshGenerationSystem::queueDirtyBuildPartMeshes */
	void ChunkMeshGenerationSystem::queueDirtyBuildPartMeshes()
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size();)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if(request.source != ChunkMeshSource::build_parts || world.chunks.isChunkActive(request.chunk_id))
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
		}

		for(ChunkId chunk_id : world.chunks.getUsedChunkIds())
		{
			Chunk& chunk = world.chunks.getChunk(chunk_id);
			if(!chunk.build_part_mesh_dirty) continue;
			world.chunks.destroyRenderData(chunk);

			ChunkMeshGenerationRequest request;
			request.source = ChunkMeshSource::build_parts;
			request.generation_id = generation_id;
			request.source_revision = chunk.build_part_mesh_revision;
			request.world_body_id = chunk.world_body_id;
			request.chunk_id = chunk_id;
			request.chunk_position = chunk.position;
			for(size_t index = 0; index < requests.size();)
			{
				const ChunkMeshGenerationRequest& queued = requests[index];
				if(queued.source != ChunkMeshSource::build_parts || queued.chunk_id != chunk_id)
				{
					index++;
					continue;
				}
				for(size_t shift = index + 1; shift < requests.size(); shift++)
					requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
				requests.resize(requests.size() - 1);
			}
			requests.pushMove(request);
			chunk.build_part_mesh_dirty = false;
			requests_dirty = true;
		}
	}

	/** @copydoc ChunkMeshGenerationSystem::removeBuildPartMeshRequest */
	void ChunkMeshGenerationSystem::removeBuildPartMeshRequest(ChunkId chunk_id)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if(request.source != ChunkMeshSource::build_parts || request.chunk_id != chunk_id) continue;
			for(size_t shift = index + 1; shift < requests.size(); shift++)
				requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(requests[shift]);
			requests.resize(requests.size() - 1);
			requests_dirty = true;
			return;
		}
	}

	/** @copydoc ChunkMeshGenerationSystem::frame */
	void ChunkMeshGenerationSystem::frame()
	{
		DSE_WATCH();
		queueDirtyBuildPartMeshes();
		if(requests_dirty)
		{
			worker->update(generation_id, requests);
			requests_dirty = false;
		}

		GeneratedChunkMesh generated;
		if(!worker->tryTake(generated) || generated.generation_id != generation_id) return;
		if(generated.source == ChunkMeshSource::build_parts)
		{
			if(!world.world_body_data.isWorldBodyActive(generated.world_body_id)
				|| !world.chunks.isChunkActive(generated.chunk_id)) return;
			Chunk& chunk = world.chunks.getChunk(generated.chunk_id);
			if(chunk.world_body_id != generated.world_body_id
				|| chunk.build_part_mesh_revision != generated.source_revision) return;
			world.chunks.createMesh(chunk,
				generated.point_positions.data(), generated.point_positions.size(),
				generated.triangle_point_indices.data(), generated.triangle_point_indices.size(),
				generated.triangle_material_ids.data(), generated.triangle_material_ids.size(), false, true);
			removeBuildPartMeshRequest(generated.chunk_id);
		}
		else if(generated.source == ChunkMeshSource::planet_terrain
			&& world.world_body_data.isWorldBodyActive(generated.world_body_id))
		{
			world.planets.acceptGeneratedChunk(generated);
		}
		else if(generated.source == ChunkMeshSource::planet_surface_lod
			&& world.world_body_data.isWorldBodyActive(generated.world_body_id))
		{
			world.planets.acceptGeneratedLodMesh(generated);
		}
	}
}
