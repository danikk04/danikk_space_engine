#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <limits>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/gui/gui.h>
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

		/** @brief Appends a fixed two-decimal value using a dot regardless of the active locale. */
		void appendGuiNumber(String& text, double value)
		{
			char buffer[64];
			const int length = std::snprintf(buffer, sizeof(buffer), "%.2f", value);
			if(length <= 0) return;
			for(int index = 0; index < length; ++index)
				if(buffer[index] == ',') buffer[index] = '.';
			text.append(buffer, static_cast<size_t>(length));
		}

		/** @brief Appends three coordinates with slash separators and fixed dot-decimal formatting. */
		void appendGuiPosition(String& text, const glm::dvec3& position)
		{
			appendGuiNumber(text, position.x);
			text << '/';
			appendGuiNumber(text, position.y);
			text << '/';
			appendGuiNumber(text, position.z);
		}

		struct SelectedUnitsSummary
		{
			uint32 count = 0;
			double total_health_percent = 0.0;
			uint32 health_samples = 0;
		};

		/** @brief Samples the selected living allied units for the summary panel. */
		SelectedUnitsSummary getSelectedUnitsSummary(const World& world)
		{
			SelectedUnitsSummary summary;
			for(const SelectedUnitRef& reference : world.controller.selected_unit_refs)
			{
				const HumanData* human_data = world.humans.findHuman(reference.human_id);
				if(human_data == nullptr || human_data->generation_id != reference.generation_id
					|| !world.world_body_data.isWorldBodyActive(human_data->world_body_id)) continue;
				const HumanState& human = *human_data;
				if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;
				++summary.count;
				const DamageableComponent& damageable = human_data->damageable;
				const float maximum_health = getDamageableDefinition(damageable.definition_id).max_health;
				if(maximum_health <= 0.0f) continue;
				summary.total_health_percent += glm::clamp(
					damageable.getHealth() / maximum_health, 0.0f, 1.0f) * 100.0;
				++summary.health_samples;
			}
			return summary;
		}

		/** @brief Writes the values shown in the debug overlay directly from their owning systems. */
		void writeWorldDebugInfo(const void* source, String& text)
		{
			if(source == nullptr)
			{
				text << "World: unavailable\n";
				return;
			}
			const World& world = *static_cast<const World*>(source);
			const glm::dvec3 camera_position = world.camera.getWorldPosition();
			const ControllerSystem& controller = world.controller;
			formatAppend(text, "MODE:%\n", controller.mode == PlayerMode::commander
				? "COMMANDER" : "GOD");
			float average_mesh_generation_time_ms = 0.0f;
			text << "MESH GEN AVG (50):";
			if(world.mesh_generation.getAverageGenerationTimeMilliseconds(
				average_mesh_generation_time_ms))
			{
				appendGuiNumber(text, average_mesh_generation_time_ms);
				text << " ms\n";
			}
			else text << " collecting\n";

			if(controller.getActiveTool().input != nullptr)
			{
				const char* size_axis_name = controller.build_part_size_axis == 0 ? "X"
					: controller.build_part_size_axis == 1 ? "Y" : "Z";
				text << "BUILD SIZE:";
				appendGuiNumber(text, controller.build_part_size.x);
				text << " x ";
				appendGuiNumber(text, controller.build_part_size.y);
				text << " x ";
				appendGuiNumber(text, controller.build_part_size.z);
				text << " m\n";
				formatAppend(text, "BUILD ROTATION:% deg (Wheel)\n",
					static_cast<uint32>(controller.build_part_rotation_step) * 90);
				formatAppend(text, "SIZE AXIS:% (X/Y/Z, Shift+Wheel)\n", size_axis_name);
			}

			text << "CAMERA SPEED:";
			appendGuiNumber(text, controller.camera_movement_speed);
			text << " m/s\n";
			double nearest_surface_distance = std::numeric_limits<double>::max();
			double nearest_planet_height = 0.0;
			bool nearest_planet_height_available = false;
			for(const PlanetData& planet : world.planets.getPlanets())
			{
				const glm::dvec3 planet_local_camera_position = glm::conjugate(planet.rotation)
					* (camera_position - planet.position);
				const double height = world.planets.getPlanetHeightAboveSurface(
					planet.id, planet_local_camera_position);
				const double distance = std::abs(height);
				if(distance >= nearest_surface_distance) continue;
				nearest_surface_distance = distance;
				nearest_planet_height = height;
				nearest_planet_height_available = true;
			}
			text << "PLANET HEIGHT:";
			if(nearest_planet_height_available) appendGuiNumber(text, nearest_planet_height);
			else text << " unavailable";
			text << " m\n";
			const glm::dvec3 truncated_camera_position = glm::trunc(camera_position * 100.0) / 100.0;
			text << "camera pos:";
			appendGuiPosition(text, truncated_camera_position);
		}

		/** @brief Formats the selected allied-unit count and average health. */
		void writeSelectedUnits(const void* source, String& text)
		{
			if(source == nullptr) return;
			const SelectedUnitsSummary summary = getSelectedUnitsSummary(
				*static_cast<const World*>(source));
			if(summary.count == 0) return;
			formatAppend(text, "UNITS: %\nAVG HP: ", summary.count);
			if(summary.health_samples == 0)
			{
				text << "--%";
				return;
			}
			formatAppend(text, "%", static_cast<uint64>(
				summary.total_health_percent / summary.health_samples + 0.5));
			text << '%';
		}

		/** @brief Reports whether any selected living allied unit should show the panel. */
		bool hasSelectedUnits(const void* source)
		{
			return source != nullptr
				&& getSelectedUnitsSummary(*static_cast<const World*>(source)).count > 0;
		}

		/** @brief Reads camera orientation and projection settings for catalog previews. */
		void readPreviewCamera(const void* source, vec3& front, vec3& up, float& vertical_fov_degrees)
		{
			if(source == nullptr) return;
			const CameraSystem& camera = *static_cast<const CameraSystem*>(source);
			front = camera.getFront();
			up = camera.getUp();
			vertical_fov_degrees = camera.vertical_fov_degrees;
		}
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
		gui.bindDebugInfo(&world, writeWorldDebugInfo);
		gui.bindSelectedUnits(&world, writeSelectedUnits, &world, hasSelectedUnits);
		gui.bindHotbar(world.controller.hotbar_entry_ids,
			&world.controller.selected_hotbar_slot, &world.controller.tool_catalog_open);
		gui.bindPreviewCamera(&world.camera, readPreviewCamera);
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
		world.telemetry.beginTick();
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
		world.telemetry.endTick();
    }
}
