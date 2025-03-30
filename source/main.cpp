#include <gui.h>
#include <keybind.h>
#include <asset.h>
#include <default.h>
#include <manager.h>
#include <localization.h>
#include <block/block.h>

namespace danikk_engine_game
{
	using namespace game;

    void pre_begin()
    {

    }

    void begin()
    {
		setWindowTitle("Sotiria Project");
		key_binds.init();
    	gui.define();
    	gui.compose();
    	static_asset_collection.load();
    	game::initLocaliztion();
    	game_manager.init();
    	game::block::initBlockTypes();
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
