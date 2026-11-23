#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/world.h>

#include <danikk_engine/input.h>
#include <danikk_framework/log.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr bool enable_tool_use_logs = true;
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

	bool updateBuildPartToolInput(ControllerSystem& controller, bool alt_pressed,
		bool control_pressed, bool shift_pressed, float mouse_scroll_delta)
	{
		DSE_PROFILE_CONTROLLER()
		if(!alt_pressed && !control_pressed && !shift_pressed)
		{
			if(getKeyboardState(keyboard_buttons::x) == button_states::press) controller.build_part_size_axis = 0;
			if(getKeyboardState(keyboard_buttons::y) == button_states::press) controller.build_part_size_axis = 1;
			if(getKeyboardState(keyboard_buttons::z) == button_states::press) controller.build_part_size_axis = 2;
			controller.build_part_size_scroll_accumulator = 0.0f;
		}
		const bool adjusting = shift_pressed && !control_pressed && !alt_pressed;
		const bool rotating = !shift_pressed && !control_pressed && !alt_pressed;
		if(adjusting)
		{
			controller.build_part_size_scroll_accumulator += mouse_scroll_delta;
			while(controller.build_part_size_scroll_accumulator >= 1.0f)
			{
				controller.build_part_size[controller.build_part_size_axis] = glm::min(
					controller.build_part_size[controller.build_part_size_axis] + 0.25f, 100.0f);
				controller.build_part_size_scroll_accumulator -= 1.0f;
			}
			while(controller.build_part_size_scroll_accumulator <= -1.0f)
			{
				controller.build_part_size[controller.build_part_size_axis] = glm::max(
					controller.build_part_size[controller.build_part_size_axis] - 0.25f, 0.25f);
				controller.build_part_size_scroll_accumulator += 1.0f;
			}
		}
		if(rotating)
		{
			controller.build_part_rotation_scroll_accumulator += mouse_scroll_delta;
			while(controller.build_part_rotation_scroll_accumulator >= 1.0f)
			{
				controller.build_part_rotation_step = (controller.build_part_rotation_step + 1) % 4;
				controller.build_part_rotation_scroll_accumulator -= 1.0f;
			}
			while(controller.build_part_rotation_scroll_accumulator <= -1.0f)
			{
				controller.build_part_rotation_step = (controller.build_part_rotation_step + 3) % 4;
				controller.build_part_rotation_scroll_accumulator += 1.0f;
			}
		}
		return adjusting;
	}

	void ControllerSystem::handleBuildPartInput(InputState& input)
	{
		DSE_PROFILE_CONTROLLER()
		const ToolInputHandler handler = getActiveTool().input;
		input.adjusting_build_part_size = handler != nullptr
			&& handler(*this, input.alt_pressed, input.control_pressed,
				input.shift_pressed, input.mouse_scroll_delta);
	}

	void updateDragTool(World& world, ControllerSystem& controller, const RaycastHit& hit,
		bool enabled, int mouse_state, float scroll_delta, bool alt_pressed,
		const vec2& cursor_delta)
	{
		tickHumanDragTool(world, controller, hit, enabled, mouse_state,
			alt_pressed ? scroll_delta : 0.0f, alt_pressed);
		const bool dragging_human = controller.dragged_human_id != static_cast<HumanId>(-1);
		tickDragTool(world, controller, hit, enabled && !dragging_human,
			mouse_state, scroll_delta, alt_pressed, cursor_delta);
	}

	bool dragToolOwnsCamera(const ControllerSystem&, bool left_mouse_down, bool alt_pressed)
	{
		return left_mouse_down && alt_pressed;
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
		const ToolCatalogEntry& active_tool = getActiveTool();
		const bool has_continuous_behavior = active_tool.tick != nullptr;
		RaycastHit hit;
		if(input.left_mouse_state == button_states::press)
			hit = world.raycasts.castFromCamera();
		const bool unit_selection_click = input.left_mouse_state == button_states::press
			&& !(mode == PlayerMode::god && has_continuous_behavior)
			&& selectOwnHumanUnderCursor(hit, input.shift_pressed);
		if(active_tool.tick != nullptr)
			active_tool.tick(world, *this, hit, mode == PlayerMode::god && input.left_mouse_down,
				input.left_mouse_state, tool_mouse_scroll_delta, input.alt_pressed,
				getCursorDelta() * getMouseSensivity());
		else
		{
			tickDragTool(world, *this, hit, false, input.left_mouse_state,
				0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, false, input.left_mouse_state, 0.0f, false);
		}
		if(mode == PlayerMode::god && !unit_selection_click
			&& active_tool.handler != nullptr && input.left_mouse_state == button_states::press)
		{
			if constexpr(enable_tool_use_logs)
				danikk_framework::formatLogInfo(
					"Tool used: name=% position=(%,%,%) hit=% terrain=%",
					active_tool.name, hit.position.x, hit.position.y, hit.position.z,
					hit.hit, hit.hit_terrain);
			applyTool(active_tool_id, world, hit, build_part_size, build_part_rotation_step);
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
		if(tool_catalog_open) return;
		validateSelectedUnits();
		handleModeInput();
		InputState input = readInputState();
		handleBuildPartInput(input);
		handleHotbarInput(input);
		handleCameraInput(input);
		handleWorldInteraction(input);
	}
}
