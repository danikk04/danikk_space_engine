#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/config.h>

namespace danikk_space_engine
{
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
}
