#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/scenario/base.h>
#include <danikk_space_engine/controller_system.h>

#include "../unit_system/internal.h"

namespace danikk_space_engine
{
	World::World()
		: chunks(*this), environment(*this), physics(*this), controller(*this), camera(*this),
		  components(*this), planets(*this), raycasts(*this)
	{
	}

	/**
	 * @brief Converts a chunk's body-local origin to double-precision world coordinates.
	 * @param chunk_id Active chunk whose body and local offset define the origin.
	 * @return Body position plus the body's rotation applied to the chunk offset.
	 */
	glm::dvec3 World::getChunkWorldPosition(ChunkId chunk_id) const
	{
		const Chunk& chunk = chunks.getChunk(chunk_id);
		const WorldBody& body = world_body_data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation * chunk.position;
	}

	/**
	 * @brief Converts a point's local position through its chunk and WorldBody transforms.
	 * @param chunk_id Active chunk containing the point.
	 * @param point_id Active point whose position is relative to that chunk.
	 * @return Double-precision position in world coordinates.
	 */
	glm::dvec3 World::getPointWorldPosition(ChunkId chunk_id, PointId point_id) const
	{
		DSE_WATCH();
		const Chunk& chunk = chunks.getChunk(chunk_id);
		const WorldBody& body = world_body_data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation
			* (chunk.position + glm::dvec3(point_data.getPoint(point_id).position));
	}

	/**
	 * @brief Adds a live point once to the queue drained after component traversal.
	 * @param point_id Point to destroy after all current component callbacks complete.
	 * Inactive IDs are ignored and duplicate requests are coalesced.
	 */
	void World::deferPointDestruction(PointId point_id)
	{
		if(!point_data.isPointActive(point_id)) return;
		for(PointId queued_point_id : pending_point_destruction_ids)
		{
			if(queued_point_id == point_id) return;
		}
		pending_point_destruction_ids.push(point_id);
	}

	/** @copydoc World::queueDirtyBuildPartMeshes */
	void World::queueDirtyBuildPartMeshes()
	{
		for(size_t index = 0; index < chunk_generation_requests.size();)
		{
			const ChunkMeshGenerationRequest& request = chunk_generation_requests[index];
			if(request.source != ChunkMeshSource::build_parts || chunks.isChunkActive(request.chunk_id))
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
				chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
		}

		for(ChunkId chunk_id : chunks.getUsedChunkIds())
		{
			Chunk& chunk = chunks.getChunk(chunk_id);
			if(!chunk.build_part_mesh_dirty) continue;
			chunks.destroyRenderData(chunk);

			ChunkMeshGenerationRequest request;
			request.source = ChunkMeshSource::build_parts;
			request.generation_id = chunk_mesh_generator_generation_id;
			request.source_revision = chunk.build_part_mesh_revision;
			request.world_body_id = chunk.world_body_id;
			request.chunk_id = chunk_id;
			request.chunk_position = chunk.position;
			for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
			{
				if(!point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part || !point_data.isPointActive(slot.point_id)) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				request.build_parts.push({part.shape, point_data.getPoint(slot.point_id).position, part.rotation, part.size});
			}

			for(size_t index = 0; index < chunk_generation_requests.size();)
			{
				const ChunkMeshGenerationRequest& queued = chunk_generation_requests[index];
				if(queued.source != ChunkMeshSource::build_parts || queued.chunk_id != chunk_id)
				{
					index++;
					continue;
				}
				for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
					chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
				chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			}
			chunk_generation_requests.pushMove(request);
			chunk.build_part_mesh_dirty = false;
			chunk_generation_requests_dirty = true;
		}
	}

	/**
	 * @brief Replaces one planet's pending requests and stamps them with the current world generation.
	 * @param world_body_id Planet body whose previous requests should be removed.
	 * @param planet_generation_id Planet incarnation that owns the request set.
	 * @param requests Latest request snapshot produced by that planet.
	 * Prior requests for this component are removed while preserving unrelated
	 * request order. New requests receive the current world token and mark the
	 * worker snapshot dirty for submission during World::frame().
	 */
	void World::replaceChunkMeshGenerationRequests(
		WorldBodyId world_body_id,
		uint64 planet_generation_id,
		const DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		for(size_t index = 0; index < chunk_generation_requests.size();)
		{
			const ChunkMeshGenerationRequest& queued_request = chunk_generation_requests[index];
			if(queued_request.source != ChunkMeshSource::planet_terrain
				|| queued_request.world_body_id != world_body_id
				|| queued_request.planet_generation_id != planet_generation_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
			{
				chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
		}
		for(const ChunkMeshGenerationRequest& request : requests)
		{
			ChunkMeshGenerationRequest queued_request = request;
			queued_request.generation_id = chunk_mesh_generator_generation_id;
			chunk_generation_requests.pushMove(queued_request);
		}
		chunk_generation_requests_dirty = true;
	}

	/**
	 * @brief Removes the queued request for one planet/grid coordinate and marks the worker queue dirty.
	 * @param world_body_id Planet body owning the request.
	 * @param planet_generation_id Planet incarnation owning the request.
	 * @param grid_coordinate Planet grid key to remove.
	 */
	void World::removeChunkMeshGenerationRequest(
		WorldBodyId world_body_id,
		uint64 planet_generation_id,
		const glm::ivec3& grid_coordinate)
	{
		for(size_t index = 0; index < chunk_generation_requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = chunk_generation_requests[index];
			if(request.source != ChunkMeshSource::planet_terrain
				|| request.world_body_id != world_body_id
				|| request.planet_generation_id != planet_generation_id
				|| request.grid_coordinate.x != grid_coordinate.x
				|| request.grid_coordinate.y != grid_coordinate.y
				|| request.grid_coordinate.z != grid_coordinate.z) continue;
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
			{
				chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
			return;
		}
	}

	/** @copydoc World::removeBuildPartMeshRequest */
	void World::removeBuildPartMeshRequest(ChunkId chunk_id)
	{
		for(size_t index = 0; index < chunk_generation_requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = chunk_generation_requests[index];
			if(request.source != ChunkMeshSource::build_parts || request.chunk_id != chunk_id) continue;
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
				chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
			return;
		}
	}



	/**
	 * @brief Starts the shared mesh generator, invalidates older results, resets queues, and creates the base scenario.
	 * Advances the world token while skipping zero, clears the submitted and local
	 * request arrays plus deferred point removals, then starts the base scenario.
	 * That scenario clears and repopulates the point and body data pools.
	 */
	void World::init()
	{
		camera.unbind();
		chunks.clear();
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			world_body_data.destroyWorldBody(body_ids[body_ids.size() - 1], chunks, planets);
		}
		startChunkMeshGenerator();
		chunk_mesh_generator_generation_id++;
		if(chunk_mesh_generator_generation_id == 0) chunk_mesh_generator_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		updateChunkMeshGenerationRequests(chunk_mesh_generator_generation_id, chunk_generation_requests);
		pending_point_destruction_ids.clear();
		unit_system_internal::resetUnitSystems();
		factions.clear();
		startBaseScenario(*this);
	}

	/**
	 * @brief Stops and joins the worker, invalidates outstanding results, and releases all resource pools.
	 * Advances the world token and clears request state before releasing resource pools.
	 */
	void World::shutdown()
	{
		camera.unbind();
		controller.unbind();
		stopChunkMeshGenerator();
		chunk_mesh_generator_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		clearBuildPartMeshCache();
		chunks.clear();
		while(world_body_data.getUsedWorldBodyIds().size() > 0)
		{
			const DynamicArray<WorldBodyId>& body_ids = world_body_data.getUsedWorldBodyIds();
			world_body_data.destroyWorldBody(body_ids[body_ids.size() - 1], chunks, planets);
		}
		point_data.release();
		world_body_data.release();
		factions.release();
		pending_point_destruction_ids.clear();
		unit_system_internal::resetUnitSystems();
	}
}
