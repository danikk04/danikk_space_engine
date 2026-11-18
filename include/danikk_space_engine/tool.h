#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	/**
	 * @brief Top-level categories used to group editing and placement actions.
	 *
	 * The current command is selected from the tool catalog and stored in a hotbar slot.
	 * `count` is a table-size sentinel and is not a selectable category.
	 */
	enum class ToolId : uint8
	{
		none = 0, ///< No action is dispatched.
		terrain, ///< Supports Extrude and Erase terrain stamps.
		place, ///< Supports Machinegun placement and primitive WorldBody creation.
		human, ///< Creates human entities.
		event, ///< Triggers a world event.
		drag, ///< Captures and moves either a human or a WorldBody.
		count ///< Number of selectable category values, including `none`.
	};

	/**
	 * @brief Actions registered within a top-level tool category.
	 *
	 * Action IDs are interpreted together with ToolId; not every action is valid in every category.
	 * Tool-catalog entries define the available category/action combinations.
	 */
	enum class ToolActionId : uint8
	{
		none = 0, ///< No action is selected or the category has no registered actions.
		extrude, ///< Adds terrain at the selected position.
		erase, ///< Removes terrain at the selected position.
		drag, ///< Captures and moves a WorldBody or human entity.
		machinegun, ///< Places a machine-gun component at a raycast hit.
		create_body, ///< Creates a WorldBody with a cube BuildPart.
		create_slope, ///< Creates a WorldBody with a slope BuildPart.
		create_human, ///< Creates a human at a supported surface hit.
		spawn_5_allies, ///< Spawns five player-faction soldiers in a column.
		spawn_5_pirates, ///< Spawns five pirate soldiers in a column.
		count ///< Number of action identifiers; not a selectable action.
	};

	/**
	 * @brief Result placeholder returned by tool dispatch.
	 */
	struct ToolApplyResult
	{
	};

	/** @brief Body-local placement data shared by event execution and its visual preview. */
	struct EventSpawnPlacement
	{
		ChunkId chunk_id = static_cast<ChunkId>(-1); ///< Loaded chunk receiving this human.
		glm::dvec3 feet_position = glm::dvec3(0.0); ///< Feet position relative to the owning WorldBody.
		vec3 up = vec3(0.0f, 0.0f, 1.0f); ///< Body-local surface normal.
		vec3 forward = vec3(0.0f, 1.0f, 0.0f); ///< Body-local initial facing direction.
	};

	/**
	 * @brief Dispatches a click/action to the handler for the selected tool category.
	 * @param tool Active category.
	 * @param action Selected action within that category.
	 * @param world World modified or queried by the action.
	 * @param hit Raycast hit used by placement and terrain actions.
	 * @param build_part_size Dimensions used when placing a BuildPart shape.
	 * @return Optional replacement face selection; unsupported category/action combinations
	 *         leave the world and selection unchanged.
	 */
	ToolApplyResult applyTool(
		ToolId tool,
		ToolActionId action,
		World& world,
		const RaycastHit& hit,
		const vec3& build_part_size);

	/**
	 * @brief Applies terrain deformation or erasure at a planet surface raycast hit.
	 * @param world World containing the hit planet component.
	 * @param action Terrain action to execute.
	 * @param hit Surface hit defining the edit center and normal.
	 * @return Tool result; terrain edits do not replace face selection.
	 */
	ToolApplyResult applyTerrainTool(World& world, ToolActionId action, const RaycastHit& hit);

	/**
	 * @brief Places a machine gun or creates a WorldBody containing the selected BuildPart shape.
	 * @param world World receiving the placed resources.
	 * @param hit Hit defining the requested placement location.
	 * @param action Placement action selected in the Place category.
	 * @param build_part_size Per-axis size used for cube and slope BuildParts.
	 * @return Tool result; placement does not replace face selection.
	 */
	ToolApplyResult applyPlaceTool(
		World& world, const RaycastHit& hit, ToolActionId action, const vec3& build_part_size);

	/**
	 * @brief Creates a human at a supported surface hit or delegates other human actions.
	 * @param world World receiving or modifying the human resources.
	 * @param hit Current camera raycast result.
	 * @param action Selected Human action.
	 * @return Tool result describing any changed face selection.
	 */
	ToolApplyResult applyHumanTool(World& world, const RaycastHit& hit, ToolActionId action);

	/** @brief Spawns a five-person allied or pirate column near the targeted surface. */
	ToolApplyResult applyEventTool(World& world, const RaycastHit& hit, ToolActionId action);
	/** @brief Finds supported positions for the selected five-person event column. */
	uint8 collectEventSpawnPlacements(
		World& world, const RaycastHit& hit, ToolActionId action, EventSpawnPlacement (&placements)[5]);

	/**
	 * @brief Updates capture, distance, and rotation controls for a dragged WorldBody.
	 * @param world World providing the camera ray and target bodies.
	 * @param controller Local controller storing capture state.
	 * @param hit Current raycast result used to begin a capture.
	 * @param enabled Whether the unified Drag tool should process a WorldBody capture.
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
	 * @param enabled Whether the unified Drag tool should process a human capture.
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
