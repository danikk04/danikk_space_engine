#include <gui.h>
#include <keybind.h>
#include <asset.h>
#include <default.h>
#include <manager.h>
#include <material.h>
#include <localization.h>
#include <block/block.h>
#include <object/tag_table.h>

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
    	generateTagsId();
    	gui.define();
    	gui.compose();
    	danikk_space_engine::initLocaliztion();
    	danikk_space_engine::initBlockTypes();
    	danikk_space_engine::initMaterials();

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
