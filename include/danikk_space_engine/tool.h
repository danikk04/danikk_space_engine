#pragma once

#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class World;
	class LocalPlayerControllerComponent;
	struct RaycastHit;

	enum class ToolId : uint8
	{
		none = 0,
		select,
		drag,
		build,
		erase_faces,
		count
	};

	enum class BuildActionId : uint8
	{
		none = 0,
		place_machinegun,
		create_world_body,
		extrude_terrain,
		count
	};

	struct ToolApplyResult
	{
		bool replace_selected_faces = false;
		DynamicArray<FacePtr> selected_faces;
	};

	ToolId stepTool(ToolId current, int direction);
	BuildActionId stepBuildAction(BuildActionId current, int direction);
	const char* getToolName(ToolId tool);
	const char* getBuildActionName(BuildActionId action);
	ToolApplyResult applyTool(
		ToolId tool,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit,
		BuildActionId build_action);
	ToolApplyResult applySelectTool(
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face);

	ToolApplyResult applyNoneTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection);
	ToolApplyResult applyEraseTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection);
	ToolApplyResult applyBuildTool(
		World& world,
		const RaycastHit& hit,
		BuildActionId action);
	ToolApplyResult applyBuildPlacementTool(World& world, const RaycastHit& hit, BuildActionId action);
	void tickDragTool(
		World& world,
		LocalPlayerControllerComponent& controller,
		const RaycastHit& hit,
		bool enabled,
		int mouse_state,
		float scroll_delta,
		bool alt_pressed,
		const vec2& cursor_delta);
}
