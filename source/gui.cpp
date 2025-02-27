#include <gui.h>
#include <asset.h>

namespace sotiria_project
{
	GUI gui;

	void GUI::define()
	{
    	fps_label.anchor = GUIAnchor::top;
    	fps_label.pos = vec2(0, 0);
    	fps_label.relative_size = vec2(0.25f, 0.05f);
	}

	void GUI::compose()
	{
    	gui_root.addChild(fps_label);
	}

	void GUI::frame()
	{
		static_asset_collection.base_shader.use();
    	format(fps_label.text, "FPS:%", fps_counter.getValue());
	}
}
