#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/keybind.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/world.h>
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
    	world.init();
    	static_asset_collection.base2d_shader.use();
    }

    void frame()
    {
    	gui.frame();
    	key_binds.frame();
    	world.tick();
    	world.frame();
    }
}
