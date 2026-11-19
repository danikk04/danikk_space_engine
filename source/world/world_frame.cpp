#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component_system.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/event_spawn_preview.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/controller_system.h>

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
		components.renderWorldBodyComponents();
		queueDirtyBuildPartMeshes();
		if(chunk_generation_requests_dirty)
		{
			updateChunkMeshGenerationRequests(chunk_mesh_generator_generation_id, chunk_generation_requests);
			chunk_generation_requests_dirty = false;
		}

		GeneratedChunkMesh generated_chunk;
		if(tryTakeGeneratedChunkMesh(generated_chunk)
			&& generated_chunk.generation_id == chunk_mesh_generator_generation_id
			&& generated_chunk.source == ChunkMeshSource::build_parts
			&& world_body_data.isWorldBodyActive(generated_chunk.world_body_id)
			&& chunks.isChunkActive(generated_chunk.chunk_id))
		{
			Chunk& chunk = chunks.getChunk(generated_chunk.chunk_id);
			if(chunk.world_body_id == generated_chunk.world_body_id
				&& chunk.build_part_mesh_revision == generated_chunk.source_revision)
			{
				chunks.createMesh(chunk,
					generated_chunk.point_positions.data(), generated_chunk.point_positions.size(),
					generated_chunk.triangle_point_indices.data(), generated_chunk.triangle_point_indices.size(),
					generated_chunk.triangle_material_ids.data(), generated_chunk.triangle_material_ids.size(),
					false, true);
				removeBuildPartMeshRequest(generated_chunk.chunk_id);
			}
		}
		else if(generated_chunk.source == ChunkMeshSource::planet_terrain
			&& generated_chunk.generation_id == chunk_mesh_generator_generation_id
			&& world_body_data.isWorldBodyActive(generated_chunk.world_body_id)
			&& world_body_data.isWorldBodyComponentSlotActive(generated_chunk.component_slot_id))
		{
			WorldBodyComponentSlot& slot = world_body_data.getWorldBodyComponentSlot(generated_chunk.component_slot_id);
			if(slot.world_body_id == generated_chunk.world_body_id
				&& slot.generation_id == generated_chunk.component_generation_id)
			{
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.accept_generated_chunk != nullptr)
				{
					type.accept_generated_chunk(slot, *this, generated_chunk);
				}
			}
		}
		static_asset_collection.base3d_shader.use();
		const CameraComponent& camera = getCameraComponent();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		for(WorldBodyId body_id : world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world_body_data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				Chunk& chunk = chunks.getChunk(chunk_id);
				chunks.draw(chunk);
			}
		}
		// Draw dynamic chunk components after the world meshes so transparent overlays
		// do not allow later terrain draws to pass through the base mesh depth.
		components.renderChunkComponents();
		if(settings.show_chunk_bounds) drawChunkBoundaries(*this);
		drawUnitSelectionIndicators(*this);
		drawUnitOrderIndicators(*this);
		const ControllerSystem& player_controller = controller;
		const bool show_build_part_preview = player_controller.active_tool == ToolId::place
			&& (player_controller.selected_tool_action == ToolActionId::create_body
				|| player_controller.selected_tool_action == ToolActionId::create_slope);
		const bool show_event_spawn_preview = player_controller.active_tool == ToolId::event
			&& (player_controller.selected_tool_action == ToolActionId::spawn_5_allies
				|| player_controller.selected_tool_action == ToolActionId::spawn_5_pirates);
		if(settings.show_world_body_preview && (show_build_part_preview || show_event_spawn_preview))
		{
			const RaycastHit hit = raycastFromCamera();
			if(show_build_part_preview)
			{
				const BuildPartShape shape = player_controller.selected_tool_action == ToolActionId::create_slope
					? BuildPartShape::slope : BuildPartShape::cube;
				drawWorldBodyPlacementPreview(
					*this, hit, shape, player_controller.build_part_size, player_controller.build_part_size_axis,
					player_controller.build_part_rotation_step);
			}
			else drawEventSpawnPreview(*this, hit, player_controller.selected_tool_action);
		}
	}
}
