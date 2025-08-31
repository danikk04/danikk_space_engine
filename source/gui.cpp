#include <gui.h>
#include <asset.h>
#include <manager.h>

#include <danikk_framework/glm.h>
#include <danikk_framework/format.h>

namespace danikk_space_engine
{
	GUI gui;

	TextStyle debug_info_label_style;

	void GUI::define()
	{
    	fps_label.anchor = GUIAnchor::top;
    	fps_label.pos = vec2(0, 0);
    	fps_label.relative_size = vec2(0.25f, 0.05f);

    	debug_info_label_style.anchor = TextAnchor::multiline;
    	debug_info_label_style.absolute_char_height = 0.01f;
    	debug_info_label_style.absolute_char_width = 0.01f;
    	debug_info_label_style.shader = static_asset_collection.base2d_shader;
    	debug_info_label_style.font = static_asset_collection.base_font;

    	debug_info_label.anchor = GUIAnchor::top_left;
    	debug_info_label.text_style = &debug_info_label_style;
    	debug_info_label.pos = vec2(0, 0);
    	debug_info_label.relative_size = vec2(0.4f, 0.3f);
	}

	void GUI::compose()
	{
    	gui_root.addChild(fps_label);
    	gui_root.addChild(debug_info_label);
	}

	void GUI::frame()
	{
    	format(fps_label.text, "FPS:%", fps_counter.getValue());
    	format(debug_info_label.text, "camera_pos:%", truncateTo100(game_manager.main_camera->pos));
	}
}
