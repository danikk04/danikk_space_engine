#include <cstdlib>
#include <cmath>
#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/assets/collection.h>
#include <danikk_space_engine/assets/human_skin.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/localization.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/scenario/npc_diagnostics.h>
#include <danikk_engine/dynamic_mesh.h>
#include <danikk_engine/input.h>
#include <danikk_framework/memory.h>

namespace danikk_engine_game
{
	using namespace danikk_space_engine;

	namespace
	{
		World world;
	}

	/** @brief Collects the World values required to render the GUI without exposing World to GUI. */
	GUIFrameData collectGUIFrameData(const World& world)
	{
		GUIFrameData data;
		data.camera_position = world.camera.getWorldPosition();
		data.average_mesh_generation_time_available =
			world.mesh_generation.getAverageGenerationTimeMilliseconds(
				data.average_mesh_generation_time_ms);
		double nearest_surface_distance = std::numeric_limits<double>::max();
		for(const PlanetData& planet : world.planets.getPlanets())
		{
			const glm::dvec3 planet_local_camera_position = glm::conjugate(planet.rotation)
				* (data.camera_position - planet.position);
			const double height = world.planets.getPlanetHeightAboveSurface(
				planet.id, planet_local_camera_position);
			const double surface_distance = std::abs(height);
			if(surface_distance >= nearest_surface_distance) continue;
			nearest_surface_distance = surface_distance;
			data.nearest_planet_height = height;
			data.nearest_planet_height_available = true;
		}
		const CameraSystem& camera = world.camera;
		data.preview_camera_front = camera.getFront();
		data.preview_camera_up = camera.getUp();
		data.preview_vertical_fov_degrees = camera.vertical_fov_degrees;
		const ControllerSystem& controller = world.controller;
		data.camera_speed = controller.camera_movement_speed;
		data.commander_mode = controller.mode == PlayerMode::commander;
		data.tool_catalog_open = controller.tool_catalog_open;
		data.active_tool = controller.active_tool;
		data.selected_tool_action = controller.selected_tool_action;
		data.build_part_size = controller.build_part_size;
		data.build_part_size_axis = controller.build_part_size_axis;
		data.build_part_rotation_step = controller.build_part_rotation_step;
		data.selected_hotbar_slot = controller.selected_hotbar_slot;
		for(uint32 index = 0; index < 36; ++index)
			data.hotbar_entry_ids[index] = controller.hotbar_entry_ids[index];

		double total_health_percent = 0.0;
		uint32 health_sample_count = 0;
		for(const SelectedUnitRef& reference : controller.selected_unit_refs)
		{
			const HumanData* human_data = world.humans.findHuman(reference.human_id);
			if(human_data == nullptr || human_data->generation_id != reference.generation_id
				|| !world.world_body_data.isWorldBodyActive(human_data->world_body_id)) continue;
			const HumanState& human = *human_data;
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;
			++data.selected_unit_count;
			const DamageableComponent* damageable = &human_data->damageable;
			const float maximum_health = getDamageableDefinition(damageable->definition_id).max_health;
			if(maximum_health <= 0.0f) continue;
			total_health_percent += glm::clamp(damageable->getHealth() / maximum_health, 0.0f, 1.0f) * 100.0;
			++health_sample_count;
		}
		if(health_sample_count > 0)
		{
			data.selected_unit_health_available = true;
			data.selected_unit_average_health_percent = static_cast<float>(total_health_percent / health_sample_count);
		}
		return data;
	}

	/** @brief Applies actions emitted by GUI widgets to the owning World's controller. */
	void applyGUIActions(World& world, GUI& gui)
	{
		for(const GUIAction& action : gui.getPendingActions())
		{
			switch(action.type)
			{
				case GUIActionType::toggle_tool_catalog:
					world.controller.tool_catalog_open = !world.controller.tool_catalog_open;
					break;
				case GUIActionType::close_tool_catalog:
					world.controller.tool_catalog_open = false;
					break;
				case GUIActionType::select_tool_catalog_entry:
					world.controller.selectToolCatalogEntry(action.catalog_entry_id);
					world.controller.tool_catalog_open = false;
					break;
				case GUIActionType::select_hotbar_slot:
					world.controller.selectHotbarSlot(action.hotbar_slot);
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
		telemetry_system.beginTick();
		{
			/**
			 * @brief Measures the duration of the enclosing telemetry scope.
			 */
			DSE_WATCH();
			gui.frame(collectGUIFrameData(world));
			applyGUIActions(world, gui);
			if(getKeyboardState(keyboard_buttons::escape) == button_states::press)
			{
				exit();
			}
			world.tick();
			world.frame();
		}
		telemetry_system.endTick();
    }
}
