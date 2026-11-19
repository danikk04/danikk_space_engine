#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component_system.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/event_spawn_preview.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/effects/unit_selection.h>
#include <danikk_space_engine/effects/unit_order_indicators.h>
#include <danikk_space_engine/effects/unit_activity_labels.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/controller_system.h>

namespace danikk_space_engine
{
	/**
	 * @brief Accepts generated work, updates chunk/component render state, and draws the world frame.
	 *
		 * Completed chunks are accepted only when world, body, planet generation,
		 * and terrain revision still match. Dirty meshes are rebuilt on
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
		planets.frame();
		chunk_mesh_generation.frame();
		static_asset_collection.base3d_shader.use();
		camera.frame();
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
			const RaycastHit hit = raycasts.castFromCamera();
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
		drawUnitActivityLabels(*this);
	}
}
