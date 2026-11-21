#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	void ControllerSystem::duplicateControllerPointWithMachinegun(const Point& source_point,
		const CameraSystem& camera, WorldBodyId body_id)
	{
		DSE_PROFILE_CONTROLLER()
		const PointId point_id = world.point_data.createPoint();
		world.point_data.getPoint(point_id) = source_point;
		world.point_data.getPoint(point_id).world_body_id = static_cast<WorldBodyId>(-1);
		world.point_data.getPoint(point_id).world_body_point_index = static_cast<size_t>(-1);
		world.point_data.getPoint(point_id).component_slot_ids.clear();
		MachinegunComponent machinegun;
		machinegun.forward = camera.getFront();
		machinegun.up = camera.getUp();
		machinegun.right = camera.getRight();
		const PointComponentSlotId slot_id = world.point_data.createPointComponentSlot(
			point_id, PointComponentTypeId::machine_gun, &machinegun);
		world.attachPointComponentToBody(body_id, point_id, slot_id, source_point.body_local_position);
	}

	void ControllerSystem::validateControllerBinding() const
	{
		DSE_PROFILE_CONTROLLER()
		if(world.camera.getPointId() != controlled_point_id)
			danikk_engine::formatFatalError("Controller system point does not match the active camera point");
	}

	bool ControllerSystem::captureDebugSpawn(Point& source_point, WorldBodyId& owner_body_id) const
	{
		DSE_PROFILE_CONTROLLER()
		if(getKeyboardState(keyboard_buttons::m) != button_states::press) return false;
		if(!world.point_data.isPointActive(controlled_point_id))
			danikk_engine::formatFatalError("Controller system references invalid point %", controlled_point_id);
		source_point = world.point_data.getPoint(controlled_point_id);
		owner_body_id = source_point.world_body_id;
		return true;
	}

	bool ControllerSystem::handleToolCatalogInput(
		bool spawn_machinegun, const Point& source_point, WorldBodyId owner_body_id)
	{
		DSE_PROFILE_CONTROLLER()
		if(!tool_catalog_open) return false;
		if(spawn_machinegun)
			duplicateControllerPointWithMachinegun(source_point, world.camera, owner_body_id);
		return true;
	}

	void ControllerSystem::handleModeInput()
	{
		DSE_PROFILE_CONTROLLER()
		if(getKeyboardState(keyboard_buttons::slash) == button_states::press)
			mode = mode == PlayerMode::god ? PlayerMode::commander : PlayerMode::god;
	}

	ControllerSystem::InputState ControllerSystem::readInputState() const
	{
		DSE_PROFILE_CONTROLLER()
		InputState input;
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const int left_control_state = getKeyboardState(keyboard_buttons::left_control);
		const int right_control_state = getKeyboardState(keyboard_buttons::right_control);
		const int left_shift_state = getKeyboardState(keyboard_buttons::left_shift);
		const int right_shift_state = getKeyboardState(keyboard_buttons::right_shift);
		input.left_mouse_state = getMouseState(mouse_buttons::left);
		input.left_mouse_down = input.left_mouse_state == button_states::press
			|| input.left_mouse_state == button_states::hold;
		input.mouse_scroll_delta = getMouseScrollDelta();
		input.alt_pressed = left_alt_state == button_states::press
			|| left_alt_state == button_states::hold
			|| right_alt_state == button_states::press
			|| right_alt_state == button_states::hold;
		input.control_pressed = left_control_state == button_states::press
			|| left_control_state == button_states::hold
			|| right_control_state == button_states::press
			|| right_control_state == button_states::hold;
		input.shift_pressed = left_shift_state == button_states::press
			|| left_shift_state == button_states::hold
			|| right_shift_state == button_states::press
			|| right_shift_state == button_states::hold;
		return input;
	}

	void ControllerSystem::handleBuildPartInput(InputState& input)
	{
		DSE_PROFILE_CONTROLLER()
		const bool placing_build_part = active_tool == ToolId::place
			&& (selected_tool_action == ToolActionId::create_body
				|| selected_tool_action == ToolActionId::create_slope);
		if(placing_build_part && !input.alt_pressed && !input.control_pressed && !input.shift_pressed)
		{
			if(getKeyboardState(keyboard_buttons::x) == button_states::press) build_part_size_axis = 0;
			if(getKeyboardState(keyboard_buttons::y) == button_states::press) build_part_size_axis = 1;
			if(getKeyboardState(keyboard_buttons::z) == button_states::press) build_part_size_axis = 2;
			build_part_size_scroll_accumulator = 0.0f;
		}
		input.adjusting_build_part_size = placing_build_part
			&& input.shift_pressed && !input.control_pressed && !input.alt_pressed;
		const bool rotating = placing_build_part
			&& !input.shift_pressed && !input.control_pressed && !input.alt_pressed;
		if(input.adjusting_build_part_size)
		{
			build_part_size_scroll_accumulator += input.mouse_scroll_delta;
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
		if(rotating)
		{
			build_part_rotation_scroll_accumulator += input.mouse_scroll_delta;
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
	}

	void ControllerSystem::handleHotbarInput(const InputState& input)
	{
		DSE_PROFILE_CONTROLLER()
		if(input.alt_pressed || (input.control_pressed && input.shift_pressed)) return;
		constexpr int hotbar_key_codes[12] =
		{
			keyboard_buttons::number1, keyboard_buttons::number2, keyboard_buttons::number3,
			keyboard_buttons::number4, keyboard_buttons::number5, keyboard_buttons::number6,
			keyboard_buttons::number7, keyboard_buttons::number8, keyboard_buttons::number9,
			keyboard_buttons::number0, keyboard_buttons::minus, keyboard_buttons::equal
		};
		const uint32 row_offset = input.control_pressed ? 24 : (input.shift_pressed ? 12 : 0);
		for(uint32 index = 0; index < 12; ++index)
			if(getKeyboardState(hotbar_key_codes[index]) == button_states::press)
			{
				selectHotbarSlot(row_offset + index);
				break;
			}
	}

	void ControllerSystem::handleCameraInput(const InputState& input)
	{
		DSE_PROFILE_CONTROLLER()
		if(input.control_pressed && input.mouse_scroll_delta != 0.0f)
			camera_movement_speed = glm::clamp(
				camera_movement_speed * std::pow(1.25f, input.mouse_scroll_delta), 0.01f, 1000000.0f);
	}

	void ControllerSystem::handleWorldInteraction(const InputState& input)
	{
		DSE_PROFILE_CONTROLLER()
		const float tool_mouse_scroll_delta = input.control_pressed || input.adjusting_build_part_size
			? 0.0f : input.mouse_scroll_delta;
		const bool drag_tool_selected = active_tool == ToolId::drag
			&& selected_tool_action == ToolActionId::drag;
		const bool drag_tool_down = mode == PlayerMode::god && drag_tool_selected && input.left_mouse_down;
		RaycastHit hit;
		if(input.left_mouse_state == button_states::press)
			hit = world.raycasts.castFromCamera();
		const bool unit_selection_click = input.left_mouse_state == button_states::press
			&& !(mode == PlayerMode::god && drag_tool_selected)
			&& selectOwnHumanUnderCursor(hit, input.shift_pressed);
		if(drag_tool_down)
		{
			tickHumanDragTool(world, *this, hit, true, input.left_mouse_state,
				input.alt_pressed ? tool_mouse_scroll_delta : 0.0f, input.alt_pressed);
			const bool dragging_human = dragged_human_slot_id != static_cast<PointComponentSlotId>(-1);
			tickDragTool(world, *this, hit, !dragging_human, input.left_mouse_state,
				tool_mouse_scroll_delta, input.alt_pressed, getCursorDelta() * getMouseSensivity());
		}
		else
		{
			tickDragTool(world, *this, hit, false, input.left_mouse_state, 0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, false, input.left_mouse_state, 0.0f, false);
			if(mode == PlayerMode::god && !unit_selection_click
				&& active_tool != ToolId::none && active_tool != ToolId::drag
				&& input.left_mouse_state == button_states::press)
			{
				applyTool(active_tool, selected_tool_action, world, hit,
					build_part_size, build_part_rotation_step);
			}
		}
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
		validateControllerBinding();
		Point source_point;
		WorldBodyId owner_body_id = static_cast<WorldBodyId>(-1);
		const bool spawn_machinegun = captureDebugSpawn(source_point, owner_body_id);
		if(handleToolCatalogInput(spawn_machinegun, source_point, owner_body_id)) return;
		validateSelectedUnits();
		handleModeInput();
		InputState input = readInputState();
		handleBuildPartInput(input);
		handleHotbarInput(input);
		handleCameraInput(input);
		handleWorldInteraction(input);
		if(spawn_machinegun)
			duplicateControllerPointWithMachinegun(source_point, world.camera, owner_body_id);
	}
}
