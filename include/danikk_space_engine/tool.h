#pragma once

#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	/**
	 * @brief Top-level categories used to group editing and placement actions.
	 *
	 * Categories cycle in declaration order, including `none`: none, terrain, body, place,
	 * and human. `count` is a table-size sentinel and is not a selectable category.
	 */
	enum class ToolId : uint8
	{
		none = 0, ///< No action is dispatched.
		terrain, ///< Supports Extrude and Erase terrain stamps.
		body, ///< Supports Extrude, Erase, and Drag for non-planet mesh bodies.
		place, ///< Supports Machinegun placement and primitive WorldBody creation.
		human, ///< Supports human Drag and Create actions.
		count ///< Number of selectable category values, including `none`.
	};

	/**
	 * @brief Actions registered within a top-level tool category.
	 *
	 * Action IDs are interpreted together with ToolId; not every action is valid in every category.
	 * The category tables define the supported subset and cycling order: Terrain has
	 * Extrude/Erase, Body has Extrude/Erase/Drag, Place has Machinegun/Body, and Human
	 * has Drag/Create. None has no action list.
	 */
	enum class ToolActionId : uint8
	{
		none = 0, ///< No action is selected or the category has no registered actions.
		extrude, ///< Adds geometry to a terrain field or extrudes a mesh face, by category.
		erase, ///< Removes terrain or a mesh face, by category.
		drag, ///< Captures and moves a WorldBody or human entity.
		machinegun, ///< Places a machine-gun component at a raycast hit.
		create_body, ///< Creates the primitive WorldBody placement preview/target.
		create_human, ///< Creates a human at a supported surface hit.
		count ///< Number of action identifiers; not a selectable action.
	};

	/**
	 * @brief Carries the optional face-selection replacement produced by a tool action.
	 *
	 * When `replace_selected_faces` is false, the controller keeps its current persistent selection.
	 * When true, `selected_faces` replaces that selection; it contains persistent handles only and
	 * does not include the transient hovered face maintained by the controller.
	 */
	struct ToolApplyResult
	{
		/** @brief Whether the controller should replace its face list with selected_faces. */
		bool replace_selected_faces = false;
		/** @brief Replacement face list, meaningful only when replace_selected_faces is true. */
		DynamicArray<FacePtr> selected_faces;
	};

	/**
	 * @brief Moves through available top-level tool categories with wraparound.
	 * @param current Current category; invalid values are handled by the implementation fallback.
	 * @param direction Negative moves backward; zero and positive values advance.
	 * @return The resulting category.
	 */
	ToolId stepTool(ToolId current, int direction);

	/**
	 * @brief Returns the first action registered for a tool category.
	 * @param tool Category whose action list is queried.
	 * @return First supported action, or ToolActionId::none for a category without actions.
	 */
	ToolActionId getFirstToolAction(ToolId tool);

	/**
	 * @brief Steps through one category's registered actions with wraparound.
	 * @param tool Category whose action list is used.
	 * @param current Current action; an unsupported action falls back to the first.
	 * @param direction Negative moves backward; zero and positive values advance.
	 * @return The resulting action, or none when the category has no actions.
	 */
	ToolActionId stepToolAction(ToolId tool, ToolActionId current, int direction);

	/**
	 * @brief Returns the display label for a top-level tool category.
	 * @param tool Tool category ID.
	 * @return Static label string, or "Unknown" for an unsupported value.
	 */
	const char* getToolName(ToolId tool);

	/**
	 * @brief Returns the display label for an action ID.
	 * @param action Action ID.
	 * @return Static label string, or "Unknown" for an unsupported value.
	 */
	const char* getToolActionName(ToolActionId action);

	/**
	 * @brief Dispatches a click/action to the handler for the selected tool category.
	 * @param tool Active category.
	 * @param action Selected action within that category.
	 * @param world World modified or queried by the action.
	 * @param selected_faces Current face list, including any hover entry after the selected prefix.
	 * @param selected_face_count Number of persistent selected entries at the start of the array;
	 *        must not exceed selected_faces.size().
	 * @param has_hovered_face Whether hovered_face contains a valid current face.
	 * @param hovered_face Face under the cursor, when available.
	 * @param hit Raycast hit used by placement and terrain actions.
	 * @return Optional replacement face selection; unsupported category/action combinations
	 *         leave the world and selection unchanged.
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
	 * @brief Applies terrain deformation or erasure at a planet surface raycast hit.
	 * @param world World containing the hit planet component.
	 * @param action Terrain action to execute.
	 * @param hit Surface hit defining the edit center and normal.
	 * @return Tool result; terrain edits do not replace face selection.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit);

	/**
	 * @brief Applies a body edit using the hovered face when present, otherwise the stored selection.
	 * @param world World containing the target mesh body.
	 * @param action Body action to execute.
	 * @param selected_faces Current face list.
	 * @param selected_face_count Number of selected entries at the list prefix.
	 * @param has_hovered_face Whether a hovered face is available.
	 * @param hovered_face Current face under the cursor.
	 * @return Updated selection state when extrusion replaces the face selection.
	 * Erase uses only the hovered face when present; without a hovered face it uses
	 * the selected prefix. Extrude currently operates on one target face.
	 */
	ToolApplyResult applyBodyTool(
		World& world,
		ToolActionId action,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face);

	/**
	 * @brief Places the selected world-body or machine-gun object at the raycast hit.
	 * @param world World receiving the placed resources.
	 * @param hit Hit defining the requested placement location.
	 * @param action Placement action selected in the Place category.
	 * @return Tool result; placement does not replace face selection.
	 */
	ToolApplyResult applyPlaceTool(World& world, const RaycastHit& hit, ToolActionId action);

	/**
	 * @brief Creates a human at a supported surface hit or delegates other human actions.
	 * @param world World receiving or modifying the human resources.
	 * @param hit Current camera raycast result.
	 * @param action Selected Human action.
	 * @return Tool result describing any changed face selection.
	 */
	ToolApplyResult applyHumanTool(World& world, const RaycastHit& hit, ToolActionId action);

	/**
	 * @brief Extrudes one editable triangle into a prism-like extension on its owning body.
	 * @param world World whose point/triangle pools are modified.
	 * @param face Source face reference.
	 * @param distance Extrusion distance along the face normal.
	 * @return Result whose replacement selection identifies the new terminal face when successful.
	 */
	ToolApplyResult applyBodyExtrudeTool(World& world, const FacePtr& face, double distance);

	/**
	 * @brief Updates capture, distance, and rotation controls for a dragged WorldBody.
	 * @param world World providing the camera ray and target bodies.
	 * @param controller Local controller storing capture state.
	 * @param hit Current raycast result used to begin a capture.
	 * @param enabled Whether the active category/action is Body -> Drag.
	 * @param mouse_state Current left mouse button state.
	 * @param scroll_delta Wheel delta for rotation or Alt-distance adjustment.
	 * @param alt_pressed Whether Alt changes wheel/mouse gestures to drag controls.
	 * @param cursor_delta Mouse displacement used to rotate the captured body.
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
	 * @brief Updates capture and camera-relative placement for a dragged human capsule.
	 * @param world World used for ray and capsule hit tests.
	 * @param controller Local controller holding the human-drag state.
	 * @param hit Current world raycast result.
	 * @param enabled Whether the Human -> Drag action is active.
	 * @param mouse_state Current left mouse button state.
	 * @param scroll_delta Wheel delta used to adjust capture distance when Alt is pressed.
	 * @param alt_pressed Enables distance adjustment while dragging.
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
