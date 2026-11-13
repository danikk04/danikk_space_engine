#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/localization.h>
#include <danikk_space_engine/material.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_engine/input.h>
#include <danikk_framework/memory.h>

namespace danikk_engine_game
{
	using namespace danikk_space_engine;

    void pre_begin()
    {

    }

    void begin()
    {
		setWindowTitle("Danikk Space Engine");
		launchCheckRSSDiff(static_asset_collection.load());
		loadMaterials();
		gui.define();
		gui.compose();
		danikk_space_engine::initLocaliztion();
		world.init();
    	static_asset_collection.base2d_shader.use();
    }

    void frame()
    {
    	gui.frame();
		if(getKeyboardState(keyboard_buttons::escape) == button_states::press)
		{
			exit();
		}
    	world.tick();
    	world.frame();
    }
}
