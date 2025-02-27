#include <gui.h>
#include <keybind.h>
#include <asset.h>
#include <default.h>
#include <manager.h>

namespace danikk_engine_game
{
	using namespace sotiria_project;

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
    	game_manager.init();
    }

    void frame()
    {
    	gui.frame();
    	key_binds.frame();
    	game_manager.tick();
    	game_manager.frame();
    }
}
