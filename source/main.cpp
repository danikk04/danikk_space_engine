#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/localization.h>
#include <danikk_space_engine/material.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_engine/input.h>
#include <danikk_framework/memory.h>

namespace danikk_engine_game
{
	using namespace danikk_space_engine;

    /**
     * @brief Performs the pre begin operation.
     */
    void pre_begin()
    {

    }

	/**
	 * @brief Performs the end operation.
	 */
	void end()
	{
		config.save();
		world.shutdown();
	}

    /**
     * @brief Performs the begin operation.
     */
    void begin()
    {
		setWindowTitle("Danikk Space Engine");
		config.load();
		launchCheckRSSDiff(static_asset_collection.load());
		loadMaterials();
		gui.define();
		gui.compose();
		danikk_space_engine::initLocaliztion();
		world.init();
    	static_asset_collection.base2d_shader.use();
    }

    /**
     * @brief Performs the frame operation.
     */
    void frame()
    {
		profiler.beginTick();
		{
			/**
			 * @brief Disables copying to ensure a measured scope is recorded once.
			 */
			Watcher frame_watcher("frame");
			{
				/**
				 * @brief Disables copying to ensure a measured scope is recorded once.
				 */
				Watcher gui_scope("GUI::frame");
				gui.frame();
			}
			if(getKeyboardState(keyboard_buttons::escape) == button_states::press)
			{
				exit();
			}
			world.tick();
			world.frame();
		}
		profiler.endTick();
    }
}
