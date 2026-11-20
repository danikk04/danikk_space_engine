#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/systems/component/component.h>
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
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_engine/danikk_engine.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

#include <limits>

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
		setClearColor(camera.isActive()
			? planets.getAtmosphereBackgroundColor(camera.getWorldPosition()) : glm::vec3(0.0f));
		components.renderWorldBodyComponents();
		planets.frame();
		planet_lod.update();
		chunk_mesh_generation.frame();
		static_asset_collection.base3d_shader.use();
		camera.frame();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);

		const glm::dvec3 camera_position = camera.getWorldPosition();
		static uint32 chunk_render_report_timestamp = 0;
		static uint64 chunk_render_candidates = 0;
		static uint64 chunk_render_drawn = 0;
		static uint64 chunk_render_missing_mesh = 0;
		static uint64 chunk_render_empty_mesh = 0;
		static uint64 chunk_render_inactive = 0;
		double far_render_distance = camera.far_clip_distance;
		double closest_planet_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : planets.getPlanets())
		{
			if(!world_body_data.isWorldBodyActive(planet.world_body_id)) continue;
			const WorldBody& body = world_body_data.getWorldBody(planet.world_body_id);
			const glm::dvec3 local_camera_position = glm::conjugate(body.rotation)
				* (camera_position - body.position);
			const double surface_height = planets.getPlanetHeightAboveSurface(
				planet.world_body_id, local_camera_position);
			closest_planet_surface_distance = glm::min(
				closest_planet_surface_distance, std::abs(surface_height));
			far_render_distance = glm::max(far_render_distance,
				glm::length(body.position - camera_position) + planet.radius);
		}
		for(WorldBodyId body_id : world_body_data.getUsedWorldBodyIds())
		{
			const WorldBody& body = world_body_data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				if(!chunks.isChunkActive(chunk_id)) continue;
				const Chunk& chunk = chunks.getChunk(chunk_id);
				if(!chunk.hasValidBounds()) continue;
				const glm::dvec3 chunk_center = body.position
					+ body.rotation * (chunk.position + glm::dvec3(chunk.bounds_center));
				far_render_distance = glm::max(far_render_distance,
					glm::length(chunk_center - camera_position) + chunk.bounds_radius);
			}
		}

		const auto drawChunkMeshes = [this]()
		{
			for(WorldBodyId body_id : world_body_data.getUsedWorldBodyIds())
			{
				const WorldBody& body = world_body_data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					chunk_render_candidates++;
					if(!chunks.isChunkActive(chunk_id))
					{
						chunk_render_inactive++;
						continue;
					}
					Chunk& chunk = chunks.getChunk(chunk_id);
					switch(chunks.draw(chunk))
					{
						case ChunkDrawResult::missing_mesh: chunk_render_missing_mesh++; break;
						case ChunkDrawResult::empty_mesh: chunk_render_empty_mesh++; break;
						case ChunkDrawResult::drawn: chunk_render_drawn++; break;
					}
				}
			}
		};

		const double near_far_distance = camera.far_clip_distance;
		if(far_render_distance > near_far_distance * 1.05)
		{
			float far_near_clip = camera.far_clip_distance * 0.95f;
			if(closest_planet_surface_distance < std::numeric_limits<double>::max())
				far_near_clip = glm::min(far_near_clip, static_cast<float>(glm::max(
					static_cast<double>(camera.near_clip_distance), closest_planet_surface_distance * 0.5)));
			const float far_clip = static_cast<float>(glm::min(
				far_render_distance * 1.02, static_cast<double>(std::numeric_limits<float>::max() / 2.0f)));
			setProjectionMatrix(glm::perspective(glm::radians(camera.vertical_fov_degrees),
				screen_ratio_gz, far_near_clip, far_clip));
			setDepthRange(0.5f, 1.0f);
			planet_lod.drawFar();
			drawChunkMeshes();

			camera.frame();
			setDepthRange(0.0f, 0.5f);
			planet_lod.drawNear();
			drawChunkMeshes();
		}
		else
		{
			setDepthRange(0.0f, 1.0f);
			planet_lod.drawFar();
			planet_lod.drawNear();
			drawChunkMeshes();
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
		const uint32 now = danikk_engine::getMsTimestamp();
		if(chunk_render_report_timestamp == 0) chunk_render_report_timestamp = now;
		if(now - chunk_render_report_timestamp >= 1000)
		{
			danikk_framework::formatLogInfo(
				"Chunk rendering (last 1 s): references=% drawn=% missing_mesh=% empty_mesh=% inactive_ids=%",
				chunk_render_candidates, chunk_render_drawn, chunk_render_missing_mesh,
				chunk_render_empty_mesh, chunk_render_inactive);
			chunk_render_report_timestamp = now;
			chunk_render_candidates = 0;
			chunk_render_drawn = 0;
			chunk_render_missing_mesh = 0;
			chunk_render_empty_mesh = 0;
			chunk_render_inactive = 0;
		}
		setDepthRange(0.0f, 1.0f);
	}
}
