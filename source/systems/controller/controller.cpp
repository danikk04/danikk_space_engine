#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/event_spawn_preview.h>
#include <danikk_space_engine/profiler.h>
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
		if(!show_build_part_preview && !show_event_spawn_preview) return;

		const RaycastHit hit = world.raycasts.castFromCamera();
		if(show_build_part_preview)
		{
			const BuildPartShape shape = selected_tool_action == ToolActionId::create_slope
				? BuildPartShape::slope : BuildPartShape::cube;
			drawWorldBodyPlacementPreview(
				world, hit, shape, build_part_size, build_part_size_axis, build_part_rotation_step);
		}
		else drawEventSpawnPreview(world, hit, selected_tool_action);
	}
}
