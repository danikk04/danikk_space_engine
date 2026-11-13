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

	void GUI::compose()
	{
    	gui_root.addChild(debug_info_label);
	}

	void GUI::frame()
	{
		memory_sample_timer += getTargetFrameDelay();
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const vec3 camera_position = truncateTo100(world.getCameraPoint().position);
		if(memory_sample_bytes == 0)
		{
			format(
				debug_info_label.text,
				"FPS:%\nRAM: unavailable\ncamera_pos:%",
				fps_counter.getValue(),
				camera_position);
			return;
		}

		const size_t whole_mebibytes = memory_sample_bytes / mib(1);
		const size_t tenths_of_mebibyte = (memory_sample_bytes % mib(1)) * 10 / mib(1);
		format(
			debug_info_label.text,
			"FPS:%\nRAM:%.% MiB\ncamera_pos:%",
			fps_counter.getValue(),
			whole_mebibytes,
			tenths_of_mebibyte,
			camera_position);
	}
}
