#include <cmath>
#include <cstdio>
#include <limits>

#include <danikk_space_engine/gui/gui.h>
#include <danikk_space_engine/gui/world_bindings.h>
#include <danikk_space_engine/systems/camera/camera.h>
#include <danikk_space_engine/systems/controller/controller.h>
#include <danikk_space_engine/systems/human/damageable.h>
#include <danikk_space_engine/systems/human/human.h>
#include <danikk_space_engine/systems/human/system.h>
#include <danikk_space_engine/systems/mesh_generation/mesh_generation.h>
#include <danikk_space_engine/systems/planet/system.h>
#include <danikk_space_engine/systems/world_body/system.h>
#include <danikk_space_engine/world.h>

namespace danikk_space_engine
{
	namespace
	{
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
			for(const SelectedUnitRef& reference : world.getControllerSystem().selected_unit_refs)
			{
				const HumanData* human_data = world.getHumanSystem().findHuman(reference.human_id);
				if(human_data == nullptr || human_data->generation_id != reference.generation_id
					|| !world.getWorldBodySystem().isWorldBodyActive(human_data->world_body_id)) continue;
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
			const glm::dvec3 camera_position = world.getCameraSystem().getWorldPosition();
			const ControllerSystem& controller = world.getControllerSystem();
			formatAppend(text, "MODE:%\n", controller.mode == PlayerMode::commander
				? "COMMANDER" : "GOD");
			float average_mesh_generation_time_ms = 0.0f;
			text << "MESH GEN AVG (50):";
			if(world.getMeshGenerationSystem().getAverageGenerationTimeMilliseconds(
				average_mesh_generation_time_ms))
			{
				appendGuiNumber(text, average_mesh_generation_time_ms);
				text << " ms\n";
			}
			else text << " collecting\n";
			double terrain_gpu_time_ms = 0.0;
			text << "TERRAIN GPU PASS:";
			if(world.getPlanetSystem().getLastTerrainGpuTimeMilliseconds(terrain_gpu_time_ms))
			{
				appendGuiNumber(text, terrain_gpu_time_ms);
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
			for(const PlanetData& planet : world.getPlanetSystem().getPlanets())
			{
				const glm::dvec3 planet_local_camera_position = glm::conjugate(planet.rotation)
					* (camera_position - planet.position);
				const double height = world.getPlanetSystem().getPlanetHeightAboveSurface(
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

	void bindWorldGUI(GUI& gui, World& world)
	{
		gui.bindDebugInfo(&world, writeWorldDebugInfo);
		gui.bindSelectedUnits(&world, writeSelectedUnits, &world, hasSelectedUnits);
		gui.bindHotbar(world.getControllerSystem().hotbar_entry_ids,
			&world.getControllerSystem().selected_hotbar_slot,
			&world.getControllerSystem().tool_catalog_open);
		gui.bindPreviewCamera(&world.getCameraSystem(), readPreviewCamera);
	}
}
