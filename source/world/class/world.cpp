#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/polygon_highlight.h>
#include <danikk_space_engine/scenario/base.h>
#include <danikk_space_engine/component/point/human/npc_controller.h>

namespace danikk_space_engine
{
	World world;

	namespace
	{

	}

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
	 * @brief Ticks body and chunk components, drains deferred point deletion, then advances physics.
	 * Body, chunk, and per-body component IDs are snapshotted before callbacks
	 * can mutate ownership lists. Point destruction is deferred until traversal
	 * ends, then WorldPhysics runs after component commands and deletions.
	 */
	void World::tick()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("WorldBody components");
			const DynamicArray<WorldBodyId> world_body_ids(data.getUsedWorldBodyIds());
			DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
			DynamicArray<ChunkId> chunk_ids;
			for(WorldBodyId body_id : world_body_ids)
			{
				if(!data.isWorldBodyActive(body_id)) continue;
				component_slot_ids.clear();
				chunk_ids.clear();
				{
					const WorldBody& body = data.getWorldBody(body_id);
					for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					{
						component_slot_ids.push(slot_id);
					}
					for(ChunkId chunk_id : body.chunk_ids)
					{
						chunk_ids.push(chunk_id);
					}
				}

				for(WorldBodyComponentSlotId slot_id : component_slot_ids)
				{
					if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
					WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
					const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
					if(type.tick != nullptr)
					{
						type.tick(slot, *this);
					}
				}
				for(ChunkId chunk_id : chunk_ids)
				{
					if(!data.isChunkActive(chunk_id)) continue;
					data.getChunk(chunk_id).tick(*this);
				}
			}
		}
		for(PointComponentSlotId slot_id : pending_npc_chunk_updates) updateNpcChunkAfterTick(*this, slot_id);
		pending_npc_chunk_updates.clear();
		for(PointId point_id : pending_point_destruction_ids)
		{
			if(data.isPointActive(point_id)) data.destroyPoint(point_id);
		}
		pending_point_destruction_ids.clear();
		physics.tick(*this);
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

	/**
	 * @brief Replaces one component's pending requests and stamps them with the current world generation.
	 * @param component_slot_id Component whose previous requests should be removed.
	 * @param requests Latest request snapshot produced by that component.
	 * Prior requests for this component are removed while preserving unrelated
	 * request order. New requests receive the current world token and mark the
	 * worker snapshot dirty for submission during World::frame().
	 */
	void World::replaceChunkGenerationRequests(
		WorldBodyComponentSlotId component_slot_id,
		const DynamicArray<ChunkGenerationRequest>& requests)
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
				chunk_generation_requests[shift - 1] = static_cast<ChunkGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
		}
		for(const ChunkGenerationRequest& request : requests)
		{
			ChunkGenerationRequest queued_request = request;
			queued_request.generation_id = chunk_loader_generation_id;
			chunk_generation_requests.pushMove(queued_request);
		}
		chunk_generation_requests_dirty = true;
	}

	/**
	 * @brief Removes the queued request for one component/grid coordinate and marks the worker queue dirty.
	 * @param component_slot_id Owner component of the request.
	 * @param grid_coordinate Planet grid key to remove.
	 */
	void World::removeChunkGenerationRequest(
		WorldBodyComponentSlotId component_slot_id,
		const glm::ivec3& grid_coordinate)
	{
		for(size_t index = 0; index < chunk_generation_requests.size(); index++)
		{
			const ChunkGenerationRequest& request = chunk_generation_requests[index];
			if(request.component_slot_id != component_slot_id
				|| request.grid_coordinate.x != grid_coordinate.x
				|| request.grid_coordinate.y != grid_coordinate.y
				|| request.grid_coordinate.z != grid_coordinate.z) continue;
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
			{
				chunk_generation_requests[shift - 1] = static_cast<ChunkGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
			return;
		}
	}

	/**
	 * @brief Accepts generated work, updates chunk/component render state, and draws the world frame.
	 *
	 * Completed chunks are accepted only when world, body, slot, component
	 * generation, and terrain revision still match. Dirty meshes are rebuilt on
	 * the main thread; rendering uses camera-relative transforms and optional
	 * debug effects are controlled by Settings.
	 */
	void World::frame()
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		DSE_WATCH();
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("WorldBody components");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
				{
					if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
					WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
					const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
					if(type.frame != nullptr)
					{
						type.frame(slot, *this);
					}
				}
			}
		}
		if(chunk_generation_requests_dirty)
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("ChunkLoader::updatePendingRequests");
			chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);
			chunk_generation_requests_dirty = false;
		}

		GeneratedChunkData generated_chunk;
		if(chunk_loader.tryTakeCompleted(generated_chunk)
			&& generated_chunk.generation_id == chunk_loader_generation_id
			&& data.isWorldBodyActive(generated_chunk.world_body_id)
			&& data.isWorldBodyComponentSlotActive(generated_chunk.component_slot_id))
		{
			WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(generated_chunk.component_slot_id);
			if(slot.world_body_id == generated_chunk.world_body_id
				&& slot.generation_id == generated_chunk.component_generation_id)
			{
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.accept_generated_chunk != nullptr)
				{
					/**
					 * @brief Measures the duration of the enclosing profiler scope.
					 */
					DSE_WATCH_SCOPE("Accept generated chunk");
					type.accept_generated_chunk(slot, *this, generated_chunk);
				}
			}
		}
		static_asset_collection.base3d_shader.use();
		const CameraComponent& camera = getCameraComponent();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("Chunk mesh and component frame");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					Chunk& chunk = data.getChunk(chunk_id);
					if(chunk.mesh_dirty) chunk.createMesh();
					chunk.frame(*this);
				}
			}
		}
		{
			/**
			 * @brief Measures the duration of the enclosing profiler scope.
			 */
			DSE_WATCH_SCOPE("Chunk draw");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					Chunk& chunk = data.getChunk(chunk_id);
					chunk.draw(*this);
				}
			}
		}
		if(settings.show_chunk_bounds) drawChunkBoundaries(*this);
		if(settings.show_face_highlight) drawPolygonHighlights(*this);
		const LocalPlayerControllerComponent& controller = getLocalPlayerControllerComponent();
		if(settings.show_world_body_preview
			&& controller.active_tool == ToolId::place
			&& (controller.selected_tool_action == ToolActionId::create_body
				|| controller.selected_tool_action == ToolActionId::create_slope))
		{
			const RaycastHit hit = raycastFromCamera();
			const BuildPartShape shape = controller.selected_tool_action == ToolActionId::create_slope
				? BuildPartShape::slope : BuildPartShape::cube;
			drawWorldBodyPlacementPreview(*this, hit, shape, controller.build_part_size, controller.build_part_size_axis);
		}
	}

	/**
	 * @brief Starts the loader, invalidates older results, resets queues, and creates the base scenario.
	 * Advances the world token while skipping zero, clears the submitted and local
	 * request arrays plus deferred point removals, then starts the base scenario.
	 * That scenario clears and repopulates WorldData.
	 */
	void World::init()
	{
		chunk_loader.start();
		chunk_loader_generation_id++;
		if(chunk_loader_generation_id == 0) chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);
		pending_point_destruction_ids.clear();
		pending_npc_chunk_updates.clear();
		startBaseScenario(*this);
	}

	/**
	 * @brief Stops and joins the worker, invalidates outstanding results, and releases all WorldData pools.
	 * Advances the world token and clears request state before releasing resource pools.
	 */
	void World::shutdown()
	{
		chunk_loader.stop();
		chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		clearBuildPartMeshCache();
		data.release();
		pending_point_destruction_ids.clear();
		pending_npc_chunk_updates.clear();
	}
}
