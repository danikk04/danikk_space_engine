#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/controller_system.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Duplicates a point and attaches a machine gun aligned to the camera basis. */
	}

	void ControllerSystem::duplicateControllerPointWithMachinegun(const Point& source_point,
		const CameraComponent& camera, ChunkId chunk_id)
	{
		const PointId point_id = world.point_data.createPoint();
		world.point_data.getPoint(point_id) = source_point;
		world.point_data.getPoint(point_id).referencing_chunk_count = 0;
		MachinegunComponent machinegun;
		machinegun.forward = camera.getFront();
		machinegun.up = camera.getUp();
		machinegun.right = camera.getRight();
		const PointComponentSlotId slot_id = world.point_data.createPointComponentSlot(
			point_id, PointComponentTypeId::machine_gun, &machinegun);
		world.chunks.addPointComponentSlotToChunk(chunk_id, slot_id);
	}

	/**
	 * @brief Polls local input, updates controller settings, and dispatches current tool actions.
	 *
	 * Ctrl-wheel changes camera speed; Shift-wheel resizes the selected BuildPart axis; the
	 * unmodified wheel rotates the placement. Number keys select hotbar slots, while mouse input
	 * drives selection, tools, and drag actions.
	 */
	void ControllerSystem::tick()
	{
		DSE_WATCH();
		if(!controls()) return;
		const PointComponentSlot& camera_slot = world.getCameraSlot();
		if(camera_slot.point_id != controlled_point_id)
			danikk_engine::formatFatalError("Controller system point does not match the active camera point");
		const bool spawn_machinegun = getKeyboardState(keyboard_buttons::m) == button_states::press;
		Point source_point;
		CameraComponent source_camera;
		const ChunkId owner_chunk_id = camera_slot.chunk_id;
		if(spawn_machinegun)
		{
			if(!world.point_data.isPointActive(controlled_point_id))
				danikk_engine::formatFatalError("Controller system references invalid point %", controlled_point_id);
			source_point = world.point_data.getPoint(controlled_point_id);
			source_camera = world.getCameraComponent();
		}
		if(tool_catalog_open)
		{
			if(spawn_machinegun)
				duplicateControllerPointWithMachinegun(source_point, source_camera, owner_chunk_id);
			return;
		}

		validateSelectedUnits();
		if(getKeyboardState(keyboard_buttons::slash) == button_states::press)
			mode = mode == PlayerMode::god ? PlayerMode::commander : PlayerMode::god;

		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const int left_control_state = getKeyboardState(keyboard_buttons::left_control);
		const int right_control_state = getKeyboardState(keyboard_buttons::right_control);
		const int left_shift_state = getKeyboardState(keyboard_buttons::left_shift);
		const int right_shift_state = getKeyboardState(keyboard_buttons::right_shift);
		const int left_mouse_state = getMouseState(mouse_buttons::left);
		const bool left_mouse_down = left_mouse_state == button_states::press
			|| left_mouse_state == button_states::hold;
		const float mouse_scroll_delta = getMouseScrollDelta();
		const bool alt_pressed = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		const bool control_pressed = left_control_state == button_states::press
			|| left_control_state == button_states::hold
			|| right_control_state == button_states::press
			|| right_control_state == button_states::hold;
		const bool shift_pressed = left_shift_state == button_states::press
			|| left_shift_state == button_states::hold
			|| right_shift_state == button_states::press
			|| right_shift_state == button_states::hold;
		const bool placing_build_part = active_tool == ToolId::place
			&& (selected_tool_action == ToolActionId::create_body
				|| selected_tool_action == ToolActionId::create_slope);
		if(placing_build_part && !alt_pressed && !control_pressed && !shift_pressed)
		{
			if(getKeyboardState(keyboard_buttons::x) == button_states::press) build_part_size_axis = 0;
			if(getKeyboardState(keyboard_buttons::y) == button_states::press) build_part_size_axis = 1;
			if(getKeyboardState(keyboard_buttons::z) == button_states::press) build_part_size_axis = 2;
			build_part_size_scroll_accumulator = 0.0f;
		}
		const bool adjusting_build_part_size = placing_build_part
			&& shift_pressed && !control_pressed && !alt_pressed;
		const bool rotating_build_part = placing_build_part
			&& !shift_pressed && !control_pressed && !alt_pressed;
		if(adjusting_build_part_size)
		{
			build_part_size_scroll_accumulator += mouse_scroll_delta;
			while(build_part_size_scroll_accumulator >= 1.0f)
			{
				build_part_size[build_part_size_axis] = glm::min(build_part_size[build_part_size_axis] + 0.25f, 100.0f);
				build_part_size_scroll_accumulator -= 1.0f;
			}
			while(build_part_size_scroll_accumulator <= -1.0f)
			{
				build_part_size[build_part_size_axis] = glm::max(build_part_size[build_part_size_axis] - 0.25f, 0.25f);
				build_part_size_scroll_accumulator += 1.0f;
			}
		}
		if(rotating_build_part)
		{
			build_part_rotation_scroll_accumulator += mouse_scroll_delta;
			while(build_part_rotation_scroll_accumulator >= 1.0f)
			{
				build_part_rotation_step = (build_part_rotation_step + 1) % 4;
				build_part_rotation_scroll_accumulator -= 1.0f;
			}
			while(build_part_rotation_scroll_accumulator <= -1.0f)
			{
				build_part_rotation_step = (build_part_rotation_step + 3) % 4;
				build_part_rotation_scroll_accumulator += 1.0f;
			}
		}
		if(!alt_pressed && !(control_pressed && shift_pressed))
		{
			constexpr int hotbar_key_codes[12] =
			{
				keyboard_buttons::number1, keyboard_buttons::number2, keyboard_buttons::number3,
				keyboard_buttons::number4, keyboard_buttons::number5, keyboard_buttons::number6,
				keyboard_buttons::number7, keyboard_buttons::number8, keyboard_buttons::number9,
				keyboard_buttons::number0, keyboard_buttons::minus, keyboard_buttons::equal
			};
			const uint32 row_offset = control_pressed ? 24 : (shift_pressed ? 12 : 0);
			for(uint32 index = 0; index < 12; ++index)
				if(getKeyboardState(hotbar_key_codes[index]) == button_states::press)
				{
					selectHotbarSlot(row_offset + index);
					break;
				}
		}
		const float tool_mouse_scroll_delta = control_pressed || adjusting_build_part_size
			? 0.0f : mouse_scroll_delta;
		const bool drag_tool_selected = active_tool == ToolId::drag
			&& selected_tool_action == ToolActionId::drag;
		const bool drag_tool_down = mode == PlayerMode::god && drag_tool_selected && left_mouse_down;
		if(control_pressed && mouse_scroll_delta != 0.0f)
			camera_movement_speed = glm::clamp(
				camera_movement_speed * std::pow(1.25f, mouse_scroll_delta), 0.01f, 1000000.0f);

		RaycastHit hit = world.raycastFromCamera();
		const bool unit_selection_click = left_mouse_state == button_states::press
			&& !(mode == PlayerMode::god && drag_tool_selected)
			&& selectOwnHumanUnderCursor(hit, shift_pressed);
		if(drag_tool_down)
		{
			tickHumanDragTool(world, *this, hit, true, left_mouse_state,
				alt_pressed ? tool_mouse_scroll_delta : 0.0f, alt_pressed);
			const bool dragging_human = dragged_human_slot_id != static_cast<PointComponentSlotId>(-1);
			tickDragTool(world, *this, hit, !dragging_human, left_mouse_state,
				tool_mouse_scroll_delta, alt_pressed, getCursorDelta() * getMouseSensivity());
		}
		else
		{
			tickDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false);
			if(mode == PlayerMode::god && !unit_selection_click
				&& active_tool != ToolId::none && active_tool != ToolId::drag
				&& left_mouse_state == button_states::press)
			{
				applyTool(active_tool, selected_tool_action, world, hit,
					build_part_size, build_part_rotation_step);
				hit = world.raycastFromCamera();
			}
		}
		if(spawn_machinegun)
			duplicateControllerPointWithMachinegun(source_point, source_camera, owner_chunk_id);
	}
}
