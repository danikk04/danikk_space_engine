#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/gui/gui.h>

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
		debug_overlay.memory_sample_timer += dt;
		if(debug_overlay.memory_sample_timer >= 1.0f)
		{
			debug_overlay.memory_sample_bytes = getProcessMemoryUsageBytes();
			debug_overlay.memory_sample_timer = 0.0f;
		}
		if(tool_catalog.read_preview_camera != nullptr)
			tool_catalog.read_preview_camera(tool_catalog.preview_camera_source, tool_catalog.preview_camera_front, tool_catalog.preview_camera_up,
				tool_catalog.preview_vertical_fov_degrees);

		const bool has_selection = selected_units.has_units != nullptr
			&& selected_units.has_units(selected_units.visibility_source);
		selected_units.panel.flags.set(GUIElementFlags::visible, has_selection);
		bool catalog_open = hotbar.catalog_open_source != nullptr && *hotbar.catalog_open_source;
		if(getKeyboardState(keyboard_buttons::g) == button_states::press)
		{
			catalog_open = !catalog_open;
			queueAction(GUIActionType::toggle_tool_catalog);
			tool_catalog.panel.flags.set(GUIElementFlags::visible, catalog_open);
			setCursorCaptured(!catalog_open);
			refreshToolCatalog();
		}
		else
		{
			setCursorCaptured(!catalog_open);
			tool_catalog.panel.flags.set(GUIElementFlags::visible, catalog_open);
		}
		refreshToolCatalog();
		const uint32 selected_slot = hotbar.selected_slot_source == nullptr
			? 0 : *hotbar.selected_slot_source;
		for(uint32 index = 0; index < gui_detail::Hotbar::column_count * gui_detail::Hotbar::row_count; ++index)
		{
			hotbar.slots[index].border_draw_style = index == selected_slot
				? &hotbar.selected_slot_border_style
				: &hotbar.slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		String world_text;
		if(debug_overlay.source.writer != nullptr)
			debug_overlay.source.writer(debug_overlay.source.source, world_text);
		const size_t first_line_end = world_text.find('\n');
		if(first_line_end != String::npos)
			text.append(world_text.data(), first_line_end + 1);
		const size_t whole_mebibytes = debug_overlay.memory_sample_bytes / mib(1);
		const size_t hundredths_of_mebibyte = (debug_overlay.memory_sample_bytes % mib(1)) * 100 / mib(1);
		if(debug_overlay.memory_sample_bytes == 0)
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
		debug_overlay.label.text = static_cast<String&&>(text);
	}
}
