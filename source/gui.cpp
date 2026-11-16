#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>

namespace danikk_space_engine
{
	GUI gui;

	/** @brief Shared style referenced by the debug label for its lifetime. */
	TextStyle debug_info_label_style;

	/** @copydoc GUI::define */
	void GUI::define()
	{
		debug_info_label_style.anchor = TextAnchor::multiline;
		debug_info_label_style.absolute_char_height = 0.014f;
		debug_info_label_style.absolute_char_width = 0.014f;
		debug_info_label_style.shader = static_asset_collection.base2d_shader;
		debug_info_label_style.font = static_asset_collection.base_font;

		debug_info_label.anchor = GUIAnchor::top_left;
		debug_info_label.text_style = &debug_info_label_style;
		debug_info_label.pos = vec2(0, 0);
		debug_info_label.relative_size = vec2(0.4f, 0.3f);
		debug_info_label.name = "debug_info_label";

		hotbar_slot_fill_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_fill_style.color = vec4(0.20f, 0.21f, 0.22f, 1.0f);
		hotbar_slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_border_style.color = vec4(0.48f, 0.49f, 0.50f, 1.0f);
		hotbar_slot_border_style.width = getPixelSize() * 2.0f;
		hotbar_selected_slot_border_style.shader = static_asset_collection.base2d_shader;
		hotbar_selected_slot_border_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);
		hotbar_selected_slot_border_style.width = getPixelSize() * 3.0f;
		hotbar_slot_text_style.shader = static_asset_collection.base2d_shader;
		hotbar_slot_text_style.font = static_asset_collection.base_font;
		hotbar_slot_text_style.anchor = TextAnchor::multiline;
		hotbar_slot_text_style.absolute_char_width = 0.005f;
		hotbar_slot_text_style.absolute_char_height = 0.010f;
		hotbar_slot_text_style.color = vec4(0.78f, 0.79f, 0.80f, 1.0f);

		constexpr float slot_height = 0.070f;
		constexpr float slot_gap = 0.0045f;
		constexpr float bottom_margin = 0.025f;
		const float slot_width = slot_height * getPixelSize().x / getPixelSize().y;
		const float horizontal_step = slot_width + slot_gap;
		constexpr float vertical_step = slot_height + slot_gap;

		for(uint32 row = 0; row < hotbar_row_count; ++row)
		{
			for(uint32 column = 0; column < hotbar_column_count; ++column)
			{
				const uint32 index = row * hotbar_column_count + column;
				GUIElement& slot = hotbar_slots[index];
				GUIElement& label = hotbar_slot_labels[index];
				slot.anchor = GUIAnchor::down;
				slot.pos = vec2(
					(static_cast<float>(column) - (hotbar_column_count - 1) * 0.5f) * horizontal_step,
					bottom_margin + row * vertical_step);
				slot.relative_size = vec2(slot_width, slot_height);
				slot.draw_style = &hotbar_slot_fill_style;
				slot.border_draw_style = &hotbar_slot_border_style;
				slot.name = "hotbar_slot";

				label.anchor = GUIAnchor::top_left;
				label.pos = vec2(0.08f, 0.08f);
				label.relative_size = vec2(1.0f);
				label.text_style = &hotbar_slot_text_style;
				label.name = "hotbar_slot_label";
				const char* modifier = "";
				switch(row)
				{
					case 1: modifier = "Shift+\n"; break;
					case 2: modifier = "Ctrl+\n"; break;
				}
				label.text << modifier << "1234567890-+"[column];
			}
		}
		for(GUIElement& slot : hotbar_slots)
		{
			slot.border_draw_style = &hotbar_slot_border_style;
		}
	}

	/** @copydoc GUI::compose */
	void GUI::compose()
	{
		gui_root.addChild(debug_info_label);
		for(GUIElement& slot : hotbar_slots)
		{
			gui_root.addChild(slot);
		}
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			hotbar_slots[index].addChild(hotbar_slot_labels[index]);
		}
	}

	/**
	 * @brief Refreshes the one-second memory sample and appends each debug value on its own line.
	 *
	 * A zero memory reading is formatted as unavailable. The camera coordinates
	 * are truncated to two decimal places before formatting. The completed
	 * framework String replaces the label text once after all fields are appended. Values are
	 * ordered as FPS, RAM, tool, action, camera speed, and camera position.
	 */
	void GUI::frame()
	{
		memory_sample_timer += getTargetFrameDelay();
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const glm::dvec3 camera_position = truncateTo100(world.getCameraWorldPosition());
		const LocalPlayerControllerComponent& controller = world.getLocalPlayerControllerComponent();
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			hotbar_slots[index].border_draw_style = index == controller.selected_hotbar_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		const char* active_tool_name = getToolName(controller.active_tool);
		const char* selected_tool_action_name = getToolActionName(controller.selected_tool_action);
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		const size_t whole_mebibytes = memory_sample_bytes / mib(1);
		const size_t tenths_of_mebibyte = (memory_sample_bytes % mib(1)) * 10 / mib(1);
		formatAppend(text,
			memory_sample_bytes == 0 ? "RAM: unavailable\n" : "RAM:%.% MiB\n",
			whole_mebibytes, tenths_of_mebibyte);

		formatAppend(text, "TOOL:%\n", active_tool_name);
		formatAppend(text, "ACTION:%\n", selected_tool_action_name);
		formatAppend(text, "CAMERA SPEED:% m/s\n", controller.camera_movement_speed);
		formatAppend(text, "camera_pos:%", camera_position);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
