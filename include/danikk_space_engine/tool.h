#pragma once

#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	enum class ToolId : uint8
	{
		none = 0,
		terrain,
		body,
		place,
		human,
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
		create_human,
		count
	};

	/**
	 * @brief Reports whether a tool action changed the world and carries any resulting selection state.
	 */
	struct ToolApplyResult
	{
		bool replace_selected_faces = false;
		DynamicArray<FacePtr> selected_faces;
	};

	/**
	 * @brief Moves to the previous or next tool and wraps at either end of the tool list.
	 */
	ToolId stepTool(ToolId current, int direction);

	/**
	 * @brief Returns the first supported action for a tool, or ToolActionId::none when the tool has no actions.
	 */
	ToolActionId getFirstToolAction(ToolId tool);

	/**
	 * @brief Moves within the selected tool’s supported actions, wrapping at the ends and falling back to the first action if current is invalid.
	 */
	ToolActionId stepToolAction(ToolId tool, ToolActionId current, int direction);

	/**
	 * @brief Returns the user-facing name for a tool ID, using “Unknown” for an unsupported value.
	 */
	const char* getToolName(ToolId tool);

	/**
	 * @brief Returns the user-facing name for an action ID, using “Unknown” for an unsupported value.
	 */
	const char* getToolActionName(ToolActionId action);

	/**
	 * @brief Applies tool.
	 */
	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit);

	/**
	 * @brief Applies terrain tool.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit);

	/**
	 * @brief Applies body tool.
	 */
	ToolApplyResult applyBodyTool(
		World& world,
		ToolActionId action,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face);

	/**
	 * @brief Applies place tool.
	 */
	ToolApplyResult applyPlaceTool(World& world, const RaycastHit& hit, ToolActionId action);

	/**
	 * @brief Applies human tool.
	 */
	ToolApplyResult applyHumanTool(World& world, const RaycastHit& hit, ToolActionId action);

	/**
	 * @brief Applies body extrude tool.
	 */
	ToolApplyResult applyBodyExtrudeTool(World& world, const FacePtr& face, double distance);

	/**
	 * @brief Runs one simulation tick for drag tool.
	 */
	void tickDragTool(
		World& world,
		LocalPlayerControllerComponent& controller,
		const RaycastHit& hit,
		bool enabled,
		int mouse_state,
		float scroll_delta,
		bool alt_pressed,
		const vec2& cursor_delta);

	/**
	 * @brief Runs one simulation tick for human drag tool.
	 */
	void tickHumanDragTool(
		World& world,
		LocalPlayerControllerComponent& controller,
		const RaycastHit& hit,
		bool enabled,
		int mouse_state,
		float scroll_delta,
		bool alt_pressed);
}
