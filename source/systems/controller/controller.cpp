#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/event_spawn_preview.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/profiler.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	void ControllerSystem::init() { danikk_framework::formatLogInfo("Init ControllerSystem"); }
	void ControllerSystem::shutdown()
	{
		danikk_framework::formatLogInfo("Shutdown ControllerSystem");
		unbind();
	}
	/** @brief Resets controller state and binds it to the supplied point in the supplied world. */
	void ControllerSystem::reset(PointId point_id)
	{
		controlled_point_id = point_id;
		mode = PlayerMode::god;
		active_tool = ToolId::none;
		selected_hotbar_slot = 0;
		for(ToolCatalogEntryId& entry_id : hotbar_entry_ids) entry_id = 0;
		tool_catalog_open = false;
		build_part_size = vec3(1.0f);
		build_part_size_axis = 2;
		build_part_size_scroll_accumulator = 0.0f;
		build_part_rotation_step = 0;
		build_part_rotation_scroll_accumulator = 0.0f;
		selected_tool_action = ToolActionId::none;
		selected_unit_refs.clear();
		camera_movement_speed = 1.0f;
		dragged_world_body_id = static_cast<WorldBodyId>(-1);
		dragged_body_distance = 0.0;
		dragged_hit_offset = glm::dvec3(0.0);
		dragged_human_point_id = static_cast<PointId>(-1);
		dragged_human_chunk_id = static_cast<ChunkId>(-1);
		dragged_human_slot_id = static_cast<PointComponentSlotId>(-1);
		dragged_human_distance = 0.0;
		dragged_human_followed_ground = true;
	}

	/** @brief Returns whether this controller is bound to the supplied world instance. */
	bool ControllerSystem::controls() const
	{
		return controlled_point_id != static_cast<PointId>(-1);
	}

	/** @brief Releases world ownership and transient selections before the world is destroyed. */
	void ControllerSystem::unbind()
	{
		if(!controls()) return;
		controlled_point_id = static_cast<PointId>(-1);
		selected_unit_refs.clear();
		dragged_world_body_id = static_cast<WorldBodyId>(-1);
		dragged_human_point_id = static_cast<PointId>(-1);
		dragged_human_chunk_id = static_cast<ChunkId>(-1);
		dragged_human_slot_id = static_cast<PointComponentSlotId>(-1);
	}

	/** @copydoc ControllerSystem::renderPreview */
	void ControllerSystem::renderPreview() const
	{
		DSE_WATCH();
		if(!settings.show_world_body_preview) return;
		const bool show_build_part_preview = active_tool == ToolId::place
			&& (selected_tool_action == ToolActionId::create_body
				|| selected_tool_action == ToolActionId::create_slope);
		const bool show_event_spawn_preview = active_tool == ToolId::event
			&& (selected_tool_action == ToolActionId::spawn_5_allies
				|| selected_tool_action == ToolActionId::spawn_5_pirates);
		const bool show_human_preview = active_tool == ToolId::human
			&& (selected_tool_action == ToolActionId::create_human
				|| selected_tool_action == ToolActionId::create_reimu
				|| selected_tool_action == ToolActionId::create_marisa
				|| selected_tool_action == ToolActionId::create_tsumi_bukuro);
		if(!show_build_part_preview && !show_event_spawn_preview && !show_human_preview) return;

		static_asset_collection.base3d_shader.use(true);
		setViewMatrix(glm::lookAt(vec3(0.0f), world.camera.getFront(), world.camera.getUp()));
		setProjectionMatrix(glm::perspective(glm::radians(world.camera.vertical_fov_degrees),
			screen_ratio_gz, world.camera.near_clip_distance, world.camera.far_clip_distance));
		const RaycastHit hit = world.raycasts.castFromCamera();
		if(show_build_part_preview)
		{
			const BuildPartShape shape = selected_tool_action == ToolActionId::create_slope
				? BuildPartShape::slope : BuildPartShape::cube;
			drawWorldBodyPlacementPreview(
				world, hit, shape, build_part_size, build_part_size_axis, build_part_rotation_step);
		}
		else if(show_event_spawn_preview) drawEventSpawnPreview(world, hit, selected_tool_action);
		else if(hit.hit && world.world_body_data.isWorldBodyActive(hit.body_id))
		{
			const WorldBody& body = world.world_body_data.getWorldBody(hit.body_id);
			const glm::dquat inverse_rotation = glm::conjugate(body.rotation);
			const glm::dvec3 local_position = inverse_rotation * (hit.position - body.position);
			vec3 up = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(world.planets.isPlanet(hit.body_id))
			{
				if(glm::length(local_position) <= 1e-6) return;
				up = vec3(glm::normalize(local_position));
			}
			if(glm::length(up) <= 1e-6f) return;
			up = glm::normalize(up);
			const vec3 local_normal = vec3(inverse_rotation * glm::dvec3(hit.normal));
			if(glm::dot(up, local_normal) < 0.5f) return;
			const vec3 forward = vec3(inverse_rotation
				* (world.camera.getWorldPosition() - hit.position));

			HumanComponent preview_human;
			preview_human.up = up;
			preview_human.setInitialForward(forward);
			switch(selected_tool_action)
			{
				case ToolActionId::create_reimu:
					preview_human.appearance.skin_id = human_skin_ids::reimu_hakurei;
					break;
				case ToolActionId::create_marisa:
					preview_human.appearance.skin_id = human_skin_ids::marisa_kirisame;
					break;
				case ToolActionId::create_tsumi_bukuro:
					preview_human.appearance.skin_id = human_skin_ids::tsumi_bukuro;
					break;
				default:
					preview_human.appearance.skin_id = human_skin_ids::usmc_marpat_woodland;
					break;
			}
			const float height = preview_human.getHeight();
			const glm::dvec3 feet_position = local_position + glm::dvec3(up * 0.02f);
			const glm::dvec3 center_world = body.position
				+ body.rotation * (feet_position + glm::dvec3(up * (height * 0.5f)));
			glm::dvec3 world_up = body.rotation * glm::dvec3(up);
			glm::dvec3 world_forward = body.rotation * glm::dvec3(preview_human.getForward());
			world_forward -= world_up * glm::dot(world_forward, world_up);
			if(glm::length(world_forward) <= 1e-6) return;
			world_forward = glm::normalize(world_forward);
			const glm::dvec3 world_right = glm::normalize(glm::cross(world_forward, world_up));
			glm::mat4 orientation(1.0f);
			orientation[0] = glm::vec4(world_right, 0.0);
			orientation[1] = glm::vec4(world_forward, 0.0);
			orientation[2] = glm::vec4(world_up, 0.0);
			const mat4 body_matrix = glm::translate(mat4(1.0f),
				vec3(center_world - world.camera.getWorldPosition())) * orientation;
			const mat4 feet_matrix = body_matrix
				* glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -0.5f))
				* glm::scale(mat4(1.0f), vec3(height));
			drawHumanAppearance(preview_human, feet_matrix, vec4(0.08f, 0.35f, 1.0f, 0.55f));
		}
	}
}
