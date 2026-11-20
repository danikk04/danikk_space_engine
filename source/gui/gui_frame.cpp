#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/gui.h>

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
	void GUI::frame(const GUIFrameData& data)
	{
		DSE_WATCH();
		frame_data = data;
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		updateMessages(dt);
		memory_sample_timer += dt;
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const glm::dvec3 camera_position = truncateTo100(data.camera_position);
		selected_units_panel.flags.set(GUIElementFlags::visible, data.selected_unit_count > 0);
		if(data.selected_unit_count > 0)
		{
			selected_units_label.text.clear();
			formatAppend(selected_units_label.text, "UNITS: %\nAVG HP: ", data.selected_unit_count);
			if(data.selected_unit_health_available)
			{
				formatAppend(selected_units_label.text, "%", static_cast<uint64>(
					data.selected_unit_average_health_percent + 0.5f));
				selected_units_label.text << '%';
			}
			else selected_units_label.text << "--%";
		}
		bool catalog_open = data.tool_catalog_open;
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
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			const ToolCatalogEntryId entry_id = data.hotbar_entry_ids[index];
			hotbar_item_labels[index].text = entry_id == no_tool_catalog_entry
				? "" : getToolCatalogEntry(entry_id).short_name;
			hotbar_slots[index].border_draw_style = index == data.selected_hotbar_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		formatAppend(text, "MODE:%\n", data.commander_mode ? "COMMANDER" : "GOD");
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

		if(data.active_tool == ToolId::place
			&& (data.selected_tool_action == ToolActionId::create_body
				|| data.selected_tool_action == ToolActionId::create_slope))
		{
			const char* size_axis_name = data.build_part_size_axis == 0 ? "X"
				: data.build_part_size_axis == 1 ? "Y" : "Z";
			text << "BUILD SIZE:";
			appendGuiNumber(text, data.build_part_size.x);
			text << " x ";
			appendGuiNumber(text, data.build_part_size.y);
			text << " x ";
			appendGuiNumber(text, data.build_part_size.z);
			text << " m\n";
			formatAppend(text, "BUILD ROTATION:% deg (Wheel)\n",
				static_cast<uint32>(data.build_part_rotation_step) * 90);
			formatAppend(text, "SIZE AXIS:% (X/Y/Z, Shift+Wheel)\n", size_axis_name);
		}
		text << "CAMERA SPEED:";
		appendGuiNumber(text, data.camera_speed);
		text << " m/s\n";
		text << "PLANET HEIGHT:";
		if(data.nearest_planet_height_available) appendGuiNumber(text, data.nearest_planet_height);
		else text << " unavailable";
		text << " m\n";
		text << "EARTH LOD:";
		if(data.earth_lod_level_available) formatAppend(text, "%", data.earth_lod_level);
		else text << " unavailable";
		text << '\n';
		text << "camera pos:";
		appendGuiPosition(text, camera_position);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
