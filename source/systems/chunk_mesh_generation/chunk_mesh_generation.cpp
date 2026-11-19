#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/chunk_mesh_generation/chunk_mesh_generation.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>

namespace danikk_space_engine
{
	/** @copydoc ChunkMeshGenerationSystem::init */
	void ChunkMeshGenerationSystem::init()
	{
		DSE_WATCH();
		startChunkMeshGenerator();
		generation_id++;
		if(generation_id == 0) generation_id++;
		requests.clear();
		requests_dirty = false;
		updateChunkMeshGenerationRequests(generation_id, requests);
	}

	/** @copydoc ChunkMeshGenerationSystem::shutdown */
	void ChunkMeshGenerationSystem::shutdown()
	{
		DSE_WATCH();
		stopChunkMeshGenerator();
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

	/** @copydoc ChunkMeshGenerationSystem::removePlanetRequest */
	void ChunkMeshGenerationSystem::removePlanetRequest(WorldBodyId world_body_id,
		uint64 planet_generation_id, const glm::ivec3& grid_coordinate)
	{
		DSE_WATCH();
		for(size_t index = 0; index < requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = requests[index];
			if(request.source != ChunkMeshSource::planet_terrain
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
		cancelChunkMeshGenerationRequests(world_body_id, planet_generation_id);
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
			for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part
					|| !world.point_data.isPointActive(slot.point_id)) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				request.build_parts.push({part.shape, world.point_data.getPoint(slot.point_id).position,
					part.rotation, part.size});
			}

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
			updateChunkMeshGenerationRequests(generation_id, requests);
			requests_dirty = false;
		}

		GeneratedChunkMesh generated;
		if(!tryTakeGeneratedChunkMesh(generated) || generated.generation_id != generation_id) return;
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
	}
}
