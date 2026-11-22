#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/gui.h>

#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>
#include <danikk_engine/input.h>

namespace danikk_space_engine
{

	/**
	 * @brief Refreshes the one-second memory sample and appends each debug value on its own line.
	 *
	 * A zero memory reading is formatted as unavailable. The camera coordinates
	 * are truncated to two decimal places before formatting. The completed
	 * framework String replaces the label text once after all fields are appended. Values are
	 * ordered as FPS and RAM, followed by the game-provided debug values.
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
		if(read_preview_camera != nullptr)
			read_preview_camera(preview_camera_source, preview_camera_front, preview_camera_up,
				preview_vertical_fov_degrees);

		const bool has_selection = has_selected_units != nullptr
			&& has_selected_units(selected_units_visibility_source);
		selected_units_panel.flags.set(GUIElementFlags::visible, has_selection);
		bool catalog_open = tool_catalog_open_source != nullptr && *tool_catalog_open_source;
		if(getKeyboardState(keyboard_buttons::g) == button_states::press)
		{
			catalog_open = !catalog_open;
			queueAction(GUIActionType::toggle_tool_catalog);
			catalog_panel.flags.set(GUIElementFlags::visible, catalog_open);
			setCursorCaptured(!catalog_open);
			refreshToolCatalog();
		}
		else
		{
			setCursorCaptured(!catalog_open);
			catalog_panel.flags.set(GUIElementFlags::visible, catalog_open);
		}
		refreshToolCatalog();
		const uint32 selected_slot = selected_hotbar_slot_source == nullptr
			? 0 : *selected_hotbar_slot_source;
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			hotbar_slots[index].border_draw_style = index == selected_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		String world_text;
		if(debug_info_source.writer != nullptr)
			debug_info_source.writer(debug_info_source.source, world_text);
		const size_t first_line_end = world_text.find('\n');
		if(first_line_end != String::npos)
			text.append(world_text.data(), first_line_end + 1);
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
		if(first_line_end != String::npos)
			text.append(world_text.data() + first_line_end + 1,
				world_text.size() - first_line_end - 1);
		else text.append(world_text);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
