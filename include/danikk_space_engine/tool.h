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
		terrain,
		body,
		place,
		count
	};

	enum class ToolActionId : uint8
	{
		none = 0,
		extrude,
		erase,
		drag,
		machinegun,
		create_body,
		npc,
		count
	};

	struct ToolApplyResult
	{
		bool replace_selected_faces = false;
		DynamicArray<FacePtr> selected_faces;
	};

	ToolId stepTool(ToolId current, int direction);
	ToolActionId getFirstToolAction(ToolId tool);
	ToolActionId stepToolAction(ToolId tool, ToolActionId current, int direction);
	const char* getToolName(ToolId tool);
	const char* getToolActionName(ToolActionId action);
	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit);
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit);
	ToolApplyResult applyBodyTool(
		World& world,
		ToolActionId action,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face);
	ToolApplyResult applyPlaceTool(World& world, const RaycastHit& hit, ToolActionId action);
	ToolApplyResult applyBodyExtrudeTool(World& world, const FacePtr& face, double distance);
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
