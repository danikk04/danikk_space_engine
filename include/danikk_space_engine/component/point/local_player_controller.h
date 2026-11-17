#pragma once

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/build_catalog.h>

#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{

	/**
	 * @brief Processes player input, hotbar selection, face selection, and active tool actions.
	 */
	class LocalPlayerControllerComponent
	{
	public:
		/**
		 * @brief Selected faces occupy the prefix; at most one non-selected hovered face follows it.
		 * At the start of each tick, any old hover suffix is removed before raycasting again.
		 */
		DynamicArray<FacePtr> selected_faces;
		/** @brief Number of committed selections at the start of `selected_faces`. */
		size_t selected_face_count = 0;
		/** @brief Currently active top-level tool, advanced by unmodified wheel scrolling. */
		ToolId active_tool = ToolId::none;
		/** @brief Fractional wheel delta retained until it reaches a whole tool-selection step. */
		float tool_scroll_accumulator = 0.0f;
		/** @brief Selected hotbar slot index in row-major order, starting at the bottom-left. */
		uint32 selected_hotbar_slot = 0;
		/** @brief Build catalog definition assigned to each of the 36 hotbar slots. */
		BuildDefinitionId hotbar_item_ids[36]{};
		/** @brief Whether the modal build-catalog GUI is open. */
		bool build_catalog_open = false;
		/** @brief Preview and next placement dimensions for the selected BuildPart shape, in metres. */
		vec3 build_part_size = vec3(1.0f);
		/** @brief Local BuildPart dimension axis currently adjusted by Shift plus mouse wheel. */
		uint8 build_part_size_axis = 2;
		/** @brief Fractional wheel delta retained until the selected dimension changes by one step. */
		float build_part_size_scroll_accumulator = 0.0f;
		/** @brief Active action within the selected tool, changed by Alt plus wheel scrolling. */
		ToolActionId selected_tool_action = ToolActionId::none;
		/** @brief Fractional Alt-wheel delta retained until it changes the selected tool action. */
		float tool_action_scroll_accumulator = 0.0f;
		/** @brief Camera travel speed in world units per second, adjustable with Ctrl plus wheel. */
		float camera_movement_speed = 1.0f;
		/** @brief World body currently held by the body drag action, or invalid ID when idle. */
		WorldBodyId dragged_world_body_id = static_cast<WorldBodyId>(-1);
		/** @brief Camera-to-hit distance retained while a world body is being dragged. */
		double dragged_body_distance = 0.0;
		/** @brief Hit offset from the dragged body's origin, preserved to avoid snapping on pickup. */
		glm::dvec3 dragged_hit_offset = glm::dvec3(0.0);
		/** @brief Point ID of the human currently controlled by the human drag action. */
		PointId dragged_human_point_id = static_cast<PointId>(-1);
		/** @brief Chunk ID expected to own the dragged human point. */
		ChunkId dragged_human_chunk_id = static_cast<ChunkId>(-1);
		/** @brief Human component slot ID validated before each drag update. */
		PointComponentSlotId dragged_human_slot_id = static_cast<PointComponentSlotId>(-1);
		/** @brief Camera-to-human distance retained during a drag. */
		double dragged_human_distance = 0.0;
		/** @brief Saves the human's follow-ground setting so drag can suspend and restore it. */
		bool dragged_human_followed_ground = true;

		/**
		 * @brief Polls input, updates raycast selection, and applies the active tool or drag action.
		 *
		 * Number keys select the bottom hotbar row, Shift-number selects the middle row, and
		 * Ctrl-number selects the top row. A populated hotbar slot activates its catalog definition;
		 * an empty slot clears the active tool. The catalog modal suspends controller input. Face
		 * selection state is kept in one array: its first `selected_face_count` entries are
		 * persistent selections and any remaining entry is the current hover target. Wheel priority
		 * is Ctrl (camera speed), then Alt (tool action), then category. Category/action cycling is
		 * suppressed during a drag; without Ctrl, Drag consumes the wheel for rotation or distance
		 * depending on Alt. Ctrl wins over Alt and still changes camera speed during a drag. A camera raycast is refreshed each tick, and a
		 * pressed left button dispatches the current tool unless a drag owns the gesture.
		 * @param world World providing camera state, raycasts, component lookup, and tool targets.
		 */
		void tick(World& world);

		/**
		 * @brief Selects a hotbar slot and activates its assigned build definition.
		 * @param slot_index Row-major slot index in the range [0, 35].
		 * An empty slot clears the active tool; a populated slot selects its catalog action.
		 */
		void selectHotbarSlot(uint32 slot_index);
	};

	/**
	 * @brief Placement-constructs the controller, copying optional initial selection and tool state.
	 * @param destination Address of storage supplied by the point-component slot.
	 * @param init_data Optional pointer to a `LocalPlayerControllerComponent`; null selects defaults.
	 */
	void constructLocalPlayerControllerComponent(void* destination, const void* init_data);

	/**
	 * @brief Move-constructs the controller payload during slot relocation.
	 * @param destination Address of replacement payload storage.
	 * @param source Existing payload, whose destructor is run after the move.
	 */
	void moveLocalPlayerControllerComponent(void* destination, void* source);

	/**
	 * @brief Runs the controller destructor and releases its selected-face array.
	 * @param data Address of the live controller payload.
	 */
	void destroyLocalPlayerControllerComponent(void* data);

	/**
	 * @brief Validates the controller slot, runs input handling, then performs deferred MG-42 spawning.
	 * @param slot Slot containing the controller and its camera point ID.
	 * @param world World used for ownership lookup, camera state, and component creation.
	 * The M key is sampled before controller input. On press, the camera point and
	 * orientation are copied before any creation, then a new point with a machine-gun
	 * component is attached to the same owning chunk after the controller tick.
	 */
	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world);
}
