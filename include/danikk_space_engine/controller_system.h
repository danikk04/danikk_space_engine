#pragma once

#include <danikk_space_engine/fwd.h>
#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/tool_catalog.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	/** @brief Player interaction mode, selected with the slash key. */
	enum class PlayerMode : uint8
	{
		god,
		commander
	};

	/** @brief Identity of a selected human incarnation, protected from slot reuse. */
	struct SelectedUnitRef
	{
		PointComponentSlotId slot_id = static_cast<PointComponentSlotId>(-1);
		uint64 generation_id = 0;
	};

	/**
	 * @brief Owns local player input, selection, tools, and the point currently under control.
	 *
	 * Controller state is independent from point-component storage. The same command system can later
	 * receive commands from local input or a network session.
	 */
	class ControllerSystem
	{
		/** @brief Owning World whose input and controlled point this system manages. */
		World& world;
		/** @brief Creates a machinegun point copy using the owning world's component pools. */
		void duplicateControllerPointWithMachinegun(const Point& source_point,
			const CameraComponent& camera, ChunkId chunk_id);
		/** @brief Removes stale, dead, or no-longer-player-owned selection references. */
		void validateSelectedUnits();
		/** @brief Selects the nearest visible player-faction human intersected by the camera ray. */
		bool selectOwnHumanUnderCursor(const RaycastHit& surface_hit, bool shift_pressed);

	public:
		/** @brief Point currently controlled by this system, or invalid when unbound. */
		PointId controlled_point_id = static_cast<PointId>(-1);
		/** @brief Current interaction mode. */
		PlayerMode mode = PlayerMode::god;
		/** @brief Tool category assigned to the selected hotbar command. */
		ToolId active_tool = ToolId::none;
		/** @brief Selected hotbar slot in row-major order, starting at the bottom-left. */
		uint32 selected_hotbar_slot = 0;
		/** @brief Tool-menu command assigned to each of the 36 hotbar slots. */
		ToolCatalogEntryId hotbar_entry_ids[36]{};
		/** @brief Whether the modal tool catalog is open. */
		bool tool_catalog_open = false;
		/** @brief Current placement dimensions for BuildPart, in metres. */
		vec3 build_part_size = vec3(1.0f);
		/** @brief BuildPart dimension axis adjusted by Shift plus mouse wheel. */
		uint8 build_part_size_axis = 2;
		/** @brief Fractional wheel delta retained for dimension adjustments. */
		float build_part_size_scroll_accumulator = 0.0f;
		/** @brief Quarter-turn angle around the placement normal, in [0, 3]. */
		uint8 build_part_rotation_step = 0;
		/** @brief Fractional wheel delta retained for placement rotation. */
		float build_part_rotation_scroll_accumulator = 0.0f;
		/** @brief Action assigned to the selected hotbar slot. */
		ToolActionId selected_tool_action = ToolActionId::none;
		/** @brief Selected player-faction humans in Commander mode. */
		DynamicArray<SelectedUnitRef> selected_unit_refs;
		/** @brief Camera travel speed in world units per second. */
		float camera_movement_speed = 1.0f;
		/** @brief WorldBody currently held by the drag action. */
		WorldBodyId dragged_world_body_id = static_cast<WorldBodyId>(-1);
		/** @brief Camera-to-hit distance retained while dragging a WorldBody. */
		double dragged_body_distance = 0.0;
		/** @brief Hit offset from a dragged body's origin. */
		glm::dvec3 dragged_hit_offset = glm::dvec3(0.0);
		/** @brief Point ID of the human captured by the drag action. */
		PointId dragged_human_point_id = static_cast<PointId>(-1);
		/** @brief Chunk expected to own the captured human point. */
		ChunkId dragged_human_chunk_id = static_cast<ChunkId>(-1);
		/** @brief Human component slot validated before each drag update. */
		PointComponentSlotId dragged_human_slot_id = static_cast<PointComponentSlotId>(-1);
		/** @brief Camera-to-human distance retained while dragging. */
		double dragged_human_distance = 0.0;
		/** @brief Saved follow-ground setting restored after the human drag ends. */
		bool dragged_human_followed_ground = true;

		/** @brief Binds the controller to a point and resets transient input state. */
		void reset(PointId point_id);
		/** @brief Returns whether a controlled point is currently bound. */
		bool controls() const;
		/** @brief Releases controller state and transient selections. */
		void unbind();
		/** @brief Processes local input and applies the selected command to the controlled point. */
		void tick();

		/** @brief Binds this controller to its owning World. */
		explicit ControllerSystem(World& world) : world(world) {}
		/** @brief Selects a hotbar slot and activates its assigned command. */
		void selectHotbarSlot(uint32 slot_index);
		/** @brief Assigns a catalog command to the selected hotbar slot and activates it. */
		void selectToolCatalogEntry(ToolCatalogEntryId entry_id);
	};

}
