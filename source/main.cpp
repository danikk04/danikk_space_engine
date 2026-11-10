#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/keybind.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/manager.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/localization.h>
#include <danikk_engine/dynamic_mesh.h>

namespace danikk_engine_game
{
	using namespace danikk_space_engine;

    void pre_begin()
    {

    }

    void begin()
    {
		setWindowTitle("Danikk Space Engine");
		key_binds.init();
    	static_asset_collection.load();
    	gui.define();
    	gui.compose();
    	danikk_space_engine::initLocaliztion();
    	danikk_space_engine::loadMaterials();

    	game_manager.init();
    	static_asset_collection.base2d_shader.use();
    }

    void frame()
    {
    	gui.frame();
    	key_binds.frame();
    	game_manager.tick();
    	game_manager.frame();
    }
}
