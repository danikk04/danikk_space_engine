#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>
#include <danikk_engine/input.h>

#include <cstdio>
#include <cstring>

namespace danikk_space_engine
{
	/** @brief Appends a fixed two-decimal value using a dot regardless of the active locale. */
	static void appendGuiNumber(String& text, double value)
	{
		char buffer[64];
		const int length = std::snprintf(buffer, sizeof(buffer), "%.2f", value);
		if(length <= 0) return;
		for(int index = 0; index < length; ++index)
			if(buffer[index] == ',') buffer[index] = '.';
		text.append(buffer, static_cast<size_t>(length));
	}

	/** @brief Appends three coordinates with slash separators and fixed dot-decimal formatting. */
	static void appendGuiPosition(String& text, const glm::dvec3& position)
	{
		appendGuiNumber(text, position.x);
		text << '/';
		appendGuiNumber(text, position.y);
		text << '/';
		appendGuiNumber(text, position.z);
	}

	/**
	 * @brief Refreshes the one-second memory sample and appends each debug value on its own line.
	 *
	 * A zero memory reading is formatted as unavailable. The camera coordinates
	 * are truncated to two decimal places before formatting. The completed
	 * framework String replaces the label text once after all fields are appended. Values are
	 * ordered as FPS, RAM, optional BuildPart controls, camera speed, and camera position.
	 */
	void GUI::frame()
	{
		DSE_WATCH();
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		updateMessages(dt);
		memory_sample_timer += dt;
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const glm::dvec3 camera_position = truncateTo100(world.getCameraWorldPosition());
		LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		if(getKeyboardState(keyboard_buttons::g) == button_states::press)
		{
			controller.tool_catalog_open = !controller.tool_catalog_open;
			refreshToolCatalog();
		}
		setCursorCaptured(!controller.tool_catalog_open);
		catalog_panel.flags.set(GUIElementFlags::visible, controller.tool_catalog_open);
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			const ToolCatalogEntryId entry_id = controller.hotbar_entry_ids[index];
			hotbar_item_labels[index].text = entry_id == no_tool_catalog_entry
				? "" : getToolCatalogEntry(entry_id).short_name;
			hotbar_slots[index].border_draw_style = index == controller.selected_hotbar_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		formatAppend(text, "MODE:%\n", controller.mode == PlayerMode::god ? "GOD" : "COMMANDER");
		const size_t whole_mebibytes = memory_sample_bytes / mib(1);
		const size_t hundredths_of_mebibyte = (memory_sample_bytes % mib(1)) * 100 / mib(1);
		if(memory_sample_bytes == 0)
			text << "RAM: unavailable\n";
		else
		{
			formatAppend(text, "RAM:%.", whole_mebibytes);
			if(hundredths_of_mebibyte < 10) text << '0';
			formatAppend(text, "% MiB\n", hundredths_of_mebibyte);
		}

		if(controller.active_tool == ToolId::place
			&& (controller.selected_tool_action == ToolActionId::create_body
				|| controller.selected_tool_action == ToolActionId::create_slope))
		{
			const char* size_axis_name = controller.build_part_size_axis == 0 ? "X"
				: controller.build_part_size_axis == 1 ? "Y" : "Z";
			text << "BUILD SIZE:";
			appendGuiNumber(text, controller.build_part_size.x);
			text << " x ";
			appendGuiNumber(text, controller.build_part_size.y);
			text << " x ";
			appendGuiNumber(text, controller.build_part_size.z);
			text << " m\n";
			formatAppend(text, "SIZE AXIS:% (X/Y/Z, Shift+Wheel)\n", size_axis_name);
		}
		text << "CAMERA SPEED:";
		appendGuiNumber(text, controller.camera_movement_speed);
		text << " m/s\n";
		text << "camera pos:";
		appendGuiPosition(text, camera_position);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
