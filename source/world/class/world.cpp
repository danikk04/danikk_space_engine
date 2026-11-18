#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/scenario/base.h>

#include "../../unit_system/internal.h"

namespace danikk_space_engine
{
	World world;


	/** @brief Resolves camera_component_slot_id to the active camera slot. */
	PointComponentSlot& World::getCameraSlot()
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	/** @brief Resolves the configured camera slot as a read-only reference. */
	const PointComponentSlot& World::getCameraSlot() const
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	/** @brief Casts the camera slot payload to its registered CameraComponent type. */
	CameraComponent& World::getCameraComponent()
	{
		return *static_cast<CameraComponent*>(getCameraSlot().getData());
	}

	/** @brief Returns the registered camera payload without permitting mutation. */
	const CameraComponent& World::getCameraComponent() const
	{
		return *static_cast<const CameraComponent*>(getCameraSlot().getData());
	}

	/** @brief Resolves the point ID stored by the active camera component slot. */
	Point& World::getCameraPoint()
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	/** @brief Returns the camera point as a read-only resource reference. */
	const Point& World::getCameraPoint() const
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	/**
	 * @brief Converts camera point coordinates from chunk-local to double-precision world space.
	 * @return Body position plus rotated chunk origin and camera-point offset.
	 */
	glm::dvec3 World::getCameraWorldPosition() const
	{
		const PointComponentSlot& camera_slot = getCameraSlot();
		return getPointWorldPosition(camera_slot.chunk_id, camera_slot.point_id);
	}

	/**
	 * @brief Converts a chunk's body-local origin to double-precision world coordinates.
	 * @param chunk_id Active chunk whose body and local offset define the origin.
	 * @return Body position plus the body's rotation applied to the chunk offset.
	 */
	glm::dvec3 World::getChunkWorldPosition(ChunkId chunk_id) const
	{
		const Chunk& chunk = data.getChunk(chunk_id);
		const WorldBody& body = data.getWorldBody(chunk.world_body_id);
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
		const Chunk& chunk = data.getChunk(chunk_id);
		const WorldBody& body = data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation
			* (chunk.position + glm::dvec3(data.getPoint(point_id).position));
	}

	/** @brief Resolves the configured local-player slot and casts its payload to the controller type. */
	LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent()
	{
		PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<LocalPlayerControllerComponent*>(slot.getData());
	}

	/** @brief Returns the configured local-player controller payload as read-only. */
	const LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent() const
	{
		const PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<const LocalPlayerControllerComponent*>(slot.getData());
	}



	/**
	 * @brief Adds a live point once to the queue drained after component traversal.
	 * @param point_id Point to destroy after all current component callbacks complete.
	 * Inactive IDs are ignored and duplicate requests are coalesced.
	 */
	void World::deferPointDestruction(PointId point_id)
	{
		if(!data.isPointActive(point_id)) return;
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
			if(request.source != ChunkMeshSource::build_parts || data.isChunkActive(request.chunk_id))
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
				chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
		}

		for(ChunkId chunk_id : data.getUsedChunkIds())
		{
			Chunk& chunk = data.getChunk(chunk_id);
			if(!chunk.build_part_mesh_dirty) continue;

			ChunkMeshGenerationRequest request;
			request.source = ChunkMeshSource::build_parts;
			request.generation_id = chunk_mesh_generator_generation_id;
			request.source_revision = chunk.build_part_mesh_revision;
			request.world_body_id = chunk.world_body_id;
			request.chunk_id = chunk_id;
			request.chunk_position = chunk.position;
			for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
			{
				if(!data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part || !data.isPointActive(slot.point_id)) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				request.build_parts.push({part.shape, data.getPoint(slot.point_id).position, part.rotation, part.size});
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
	 * @brief Replaces one component's pending requests and stamps them with the current world generation.
	 * @param component_slot_id Component whose previous requests should be removed.
	 * @param requests Latest request snapshot produced by that component.
	 * Prior requests for this component are removed while preserving unrelated
	 * request order. New requests receive the current world token and mark the
	 * worker snapshot dirty for submission during World::frame().
	 */
	void World::replaceChunkMeshGenerationRequests(
		WorldBodyComponentSlotId component_slot_id,
		const DynamicArray<ChunkMeshGenerationRequest>& requests)
	{
		for(size_t index = 0; index < chunk_generation_requests.size();)
		{
			if(chunk_generation_requests[index].component_slot_id != component_slot_id)
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

	/** @copydoc World::destroyWorldBody */
	void World::destroyWorldBody(WorldBodyId body_id)
	{
		if(!data.isWorldBodyActive(body_id)) return;
		DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
		for(WorldBodyComponentSlotId slot_id : data.getWorldBody(body_id).component_slot_ids)
			component_slot_ids.push(slot_id);

		for(WorldBodyComponentSlotId slot_id : component_slot_ids)
		{
			if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
			const uint64 component_generation_id = data.getWorldBodyComponentSlot(slot_id).generation_id;
			cancelChunkMeshGenerationRequests(slot_id, component_generation_id);
			for(size_t index = 0; index < chunk_generation_requests.size();)
			{
				const ChunkMeshGenerationRequest& request = chunk_generation_requests[index];
				if(request.source != ChunkMeshSource::planet_terrain
					|| request.component_slot_id != slot_id
					|| request.component_generation_id != component_generation_id)
				{
					++index;
					continue;
				}
				for(size_t shift = index + 1; shift < chunk_generation_requests.size(); ++shift)
					chunk_generation_requests[shift - 1] = static_cast<ChunkMeshGenerationRequest&&>(chunk_generation_requests[shift]);
				chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
				chunk_generation_requests_dirty = true;
			}
		}
		data.destroyWorldBody(body_id);
	}

	/**
	 * @brief Removes the queued request for one component/grid coordinate and marks the worker queue dirty.
	 * @param component_slot_id Owner component of the request.
	 * @param grid_coordinate Planet grid key to remove.
	 */
	void World::removeChunkMeshGenerationRequest(
		WorldBodyComponentSlotId component_slot_id,
		const glm::ivec3& grid_coordinate)
	{
		for(size_t index = 0; index < chunk_generation_requests.size(); index++)
		{
			const ChunkMeshGenerationRequest& request = chunk_generation_requests[index];
			if(request.source != ChunkMeshSource::planet_terrain
				|| request.component_slot_id != component_slot_id
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
	 * That scenario clears and repopulates WorldData.
	 */
	void World::init()
	{
		startChunkMeshGenerator();
		chunk_mesh_generator_generation_id++;
		if(chunk_mesh_generator_generation_id == 0) chunk_mesh_generator_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		updateChunkMeshGenerationRequests(chunk_mesh_generator_generation_id, chunk_generation_requests);
		pending_point_destruction_ids.clear();
		pending_npc_chunk_updates.clear();
		unit_system_internal::resetUnitSystems();
		startBaseScenario(*this);
	}

	/**
	 * @brief Stops and joins the worker, invalidates outstanding results, and releases all WorldData pools.
	 * Advances the world token and clears request state before releasing resource pools.
	 */
	void World::shutdown()
	{
		stopChunkMeshGenerator();
		chunk_mesh_generator_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		clearBuildPartMeshCache();
		data.release();
		pending_point_destruction_ids.clear();
		pending_npc_chunk_updates.clear();
		unit_system_internal::resetUnitSystems();
	}
}
