#include <cstdlib>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/gui/gui.h>
#include <danikk_space_engine/gui/world_bindings.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/localization.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/scenario/npc_diagnostics.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_engine/input.h>
#include <danikk_framework/memory.h>
#include <danikk_space_engine/systems/controller/controller.h>

namespace danikk_engine_game
{
	using namespace danikk_space_engine;

	namespace
	{
		World world;
	}

	/** @brief Applies actions emitted by GUI widgets to the owning World's controller. */
	void applyGUIActions(World& world, GUI& gui)
	{
		for(const GUIAction& action : gui.getPendingActions())
		{
			switch(action.type)
			{
				case GUIActionType::toggle_tool_catalog:
					world.getControllerSystem().tool_catalog_open = !world.getControllerSystem().tool_catalog_open;
					break;
				case GUIActionType::close_tool_catalog:
					world.getControllerSystem().tool_catalog_open = false;
					break;
				case GUIActionType::select_tool_catalog_entry:
					world.getControllerSystem().selectToolCatalogEntry(action.catalog_entry_id);
					world.getControllerSystem().tool_catalog_open = false;
					break;
				case GUIActionType::select_hotbar_slot:
					world.getControllerSystem().selectHotbarSlot(action.hotbar_slot);
					break;
			}
		}
		gui.clearPendingActions();
	}

    /**
     * @brief Runs the opt-in NPC measurements before graphics and asset initialization.
     */
    void pre_begin()
    {
		const char* diagnostics = std::getenv("DSE_MEASURE_NPCS");
		if(diagnostics != nullptr && diagnostics[0] == '1' && diagnostics[1] == '\0')
			std::exit(runNpcDiagnostics());
    }

	/**
	 * @brief Saves configuration and shuts down world-owned resources.
	 *
	 * Configuration is persisted before World::shutdown() stops and joins the mesh worker, rejects
	 * outstanding generation results, and releases meshes while the graphics context is still alive.
	 */
	void end()
	{
		config.save();
		world.shutdown();
	}

    /**
	 * @brief Attempts to load settings, initializes shared assets and UI, then creates the world.
	 * @details Graphics context creation is managed by the framework before this callback. The
	 * callback sets the window title, attempts to load `settings.cfg`, loads shared assets,
	 * item/human catalogs and human skins, then loads built-in materials. It next defines and composes the debug
	 * label, registers localization strings, initializes World and its base scenario, and finally
	 * selects the base 2D shader for GUI rendering. A missing settings file leaves the defaults in
	 * use. The return value of `config.load()` is not checked here, so startup continues after a
	 * configuration read or parse error.
     */
    void begin()
    {
		setWindowTitle("Danikk Space Engine");
		config.load();
		launchCheckRSSDiff(static_asset_collection.load());
		loadHumanSkins();
		loadMaterials();
		gui.define();
		gui.compose();
		gui.addMessage("Привет, мир!");
		danikk_space_engine::initLocaliztion();
		world.init();
		bindWorldGUI(gui, world);
    	static_asset_collection.base2d_shader.use();
    }

    /**
     * @brief Executes one application frame in GUI, input, simulation, and rendering order.
	 * @details Begins the telemetry tick and its outer `frame` scope, updates the GUI, checks Escape,
	 * advances `World::tick()`, renders via `World::frame()`, then closes the scope and telemetry report.
	 * tick. The Escape check occurs between GUI refresh and world simulation.
     */
    void frame()
    {
		world.getTelemetrySystem().beginTick();
		{
			/**
			 * @brief Measures the duration of the enclosing telemetry scope.
			 */
			DSE_WATCH();
			gui.frame();
			applyGUIActions(world, gui);
			if(getKeyboardState(keyboard_buttons::escape) == button_states::press)
			{
				exit();
			}
			world.tick();
			world.frame();
		}
		world.getTelemetrySystem().endTick();
    }
}
