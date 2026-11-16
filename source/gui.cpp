#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>

namespace danikk_space_engine
{
	GUI gui;

	TextStyle debug_info_label_style;

	/**
	 * @brief Configures the debug label’s font, multiline layout, character size, and position.
	 */
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
	}

	/**
	 * @brief Adds the debug information label to the GUI hierarchy.
	 */
	void GUI::compose()
	{
    	gui_root.addChild(debug_info_label);
	}

	/**
	 * @brief Refreshes the memory sample and formats FPS, memory, tool, speed, and camera position.
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
