#pragma once

#include <danikk_space_engine/default.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/geometry/build_part.h>
#include <danikk_framework/dynamic_array.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	/**
	 * @brief Result placeholder returned by tool dispatch.
	 */
	struct ToolApplyResult
	{
	};

	/** @brief Callback invoked when a tool is activated. */
	using ToolHandler = ToolApplyResult(*)(World&, const RaycastHit&, const vec3&, uint8);
	/** @brief Callback that draws a tool's world-space placement or action preview. */
	using ToolEffectHandler = void(*)(World&, const ControllerSystem&, const RaycastHit&);
	/** @brief Callback that processes tool-specific controls and returns wheel-consumption state. */
	using ToolInputHandler = bool(*)(ControllerSystem&, bool, bool, bool, float);
	/** @brief Callback for continuous tool behavior such as dragging. */
	using ToolTickHandler = void(*)(World&, ControllerSystem&, const RaycastHit&, bool,
		int, float, bool, const vec2&);
	/** @brief Callback indicating whether a tool currently owns camera rotation input. */
	using ToolCameraHandler = bool(*)(const ControllerSystem&, bool, bool);
	/** @brief Callback for the miniature preview drawn in the tool catalog. */
	using ToolCatalogEffectHandler = void(*)(const vec3&, const vec3&, float,
		const vec2&, const vec2&);

	/** @brief Body-local placement data shared by event execution and its visual preview. */
	struct EventSpawnPlacement
	{
		WorldBodyId body_id = static_cast<WorldBodyId>(-1); ///< WorldBody receiving this human.
		glm::dvec3 feet_position = glm::dvec3(0.0); ///< Feet position relative to the owning WorldBody.
		vec3 up = vec3(0.0f, 0.0f, 1.0f); ///< Body-local surface normal.
		vec3 forward = vec3(0.0f, 1.0f, 0.0f); ///< Body-local initial facing direction.
	};

	/** @brief Shared world/body-local transform used by BuildPart preview and placement. */
	struct BuildPartPlacement
	{
		/** @brief World-space center of the proposed BuildPart. */
		glm::dvec3 world_position = glm::dvec3(0.0);
		/** @brief Body-local orientation stored on the BuildPart. */
		mat4 body_local_rotation = mat4(1.0f);
		/** @brief World-space orientation used to render the placement preview. */
		mat4 world_rotation = mat4(1.0f);
		/** @brief Existing body to attach to, or invalid when placement creates a new body. */
		WorldBodyId body_id = static_cast<WorldBodyId>(-1);
		/** @brief BuildObject receiving the new part, or invalid when placement creates one. */
		BuildObjectId build_object_id = static_cast<BuildObjectId>(-1);
		/** @brief Hit BuildPart used as the local grid reference; not a structural parent. */
		BuildPartId reference_build_part_id = static_cast<BuildPartId>(-1);
	};

	/**
	 * @brief Computes a BuildPart transform snapped to the parent grid and a quarter-turn angle.
	 * @param world World used to find the hit BuildPart's center and body transform.
	 * @param hit Current placement raycast; BuildPart hits provide the grid origin.
	 * @param shape Shape whose support extent determines the face-to-face offset.
	 * @param size Dimensions of the proposed shape in metres.
	 * @param rotation_step Number of positive quarter-turns around the placement normal, modulo four.
	 * @return Shared world/body-local transform for preview and creation.
	 */
	BuildPartPlacement getBuildPartPlacement(
		World& world, const RaycastHit& hit, BuildPartShape shape,
		const vec3& size, uint8 rotation_step);

	/**
	 * @brief Dispatches to the handler stored at the selected tool-catalog index.
	 * @param tool_id Selected registry index; zero is the inert entry.
	 * @param world World modified or queried by the action.
	 * @param hit Raycast hit used by placement and terrain actions.
	 * @param build_part_size Dimensions used when placing a BuildPart shape.
	 * @param build_part_rotation_step Snapped quarter-turn applied to BuildPart placement.
	 * @return Optional replacement face selection; an inert entry leaves state unchanged.
	 */
	ToolApplyResult applyTool(
		ToolCatalogEntryId tool_id,
		World& world,
		const RaycastHit& hit,
		const vec3& build_part_size,
		uint8 build_part_rotation_step);

	/**
	 * @brief Adds stone terrain at a planet surface raycast hit.
	 * @param hit Surface hit defining the edit center and normal.
	 * @return Tool result; terrain edits do not replace face selection.
	 */
	ToolApplyResult applyTerrainExtrudeTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applyTerrainEraseTool(World&, const RaycastHit&, const vec3&, uint8);

	/**
	 * @brief Creates a WorldBody containing one cube or slope BuildPart.
	 */
	ToolApplyResult applyCreateBodyTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applyCreateSlopeTool(World&, const RaycastHit&, const vec3&, uint8);

	/**
	 * @brief Creates the requested human variant at a supported surface hit.
	 */
	ToolApplyResult applyCreateHumanTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applyCreateReimuTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applyCreateMarisaTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applyCreateTsumiBukuroTool(World&, const RaycastHit&, const vec3&, uint8);

	/** @brief Spawns a five-person allied or pirate column near the targeted surface. */
	ToolApplyResult applySpawnAlliesTool(World&, const RaycastHit&, const vec3&, uint8);
	ToolApplyResult applySpawnPiratesTool(World&, const RaycastHit&, const vec3&, uint8);
	/** @brief Finds supported positions for the selected five-person event column. */
	uint8 collectEventSpawnPlacements(
		World& world, const RaycastHit& hit, FactionId faction_id, EventSpawnPlacement (&placements)[5]);

	void drawCubeToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawSlopeToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawHumanToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawReimuToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawMarisaToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawTsumiBukuroToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawAlliesToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	void drawPiratesToolEffect(World&, const ControllerSystem&, const RaycastHit&);
	bool updateBuildPartToolInput(ControllerSystem&, bool, bool, bool, float);
	void updateDragTool(World&, ControllerSystem&, const RaycastHit&, bool,
		int, float, bool, const vec2&);
	bool dragToolOwnsCamera(const ControllerSystem&, bool, bool);
	void drawCubeCatalogEffect(const vec3&, const vec3&, float, const vec2&, const vec2&);
	void drawSlopeCatalogEffect(const vec3&, const vec3&, float, const vec2&, const vec2&);

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
		ControllerSystem& controller,
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
		ControllerSystem& controller,
		const RaycastHit& hit,
		bool enabled,
		int mouse_state,
		float scroll_delta,
		bool alt_pressed);
}
