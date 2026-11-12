#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/world.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/format.h>

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
		format(
			debug_info_label.text,
			"FPS:%\ncamera_pos:%",
			fps_counter.getValue(),
			truncateTo100(world.data.getPoint(world.controller.target_point_id).position));
	}
}
