#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/gui.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/human/human.h>
#include <danikk_space_engine/factions.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/controller_system.h>

#include <danikk_framework/format.h>
#include <danikk_framework/memory.h>
#include <danikk_engine/input.h>

#include <cstdio>
#include <cstring>

namespace danikk_space_engine
{
	/** @brief Appends a fixed two-decimal value using a dot regardless of the active locale. */
	static void appendGuiNumber(String& text, double value)
	{
		char buffer[64];
		const int length = std::snprintf(buffer, sizeof(buffer), "%.2f", value);
		if(length <= 0) return;
		for(int index = 0; index < length; ++index)
			if(buffer[index] == ',') buffer[index] = '.';
		text.append(buffer, static_cast<size_t>(length));
	}

	/** @brief Appends three coordinates with slash separators and fixed dot-decimal formatting. */
	static void appendGuiPosition(String& text, const glm::dvec3& position)
	{
		appendGuiNumber(text, position.x);
		text << '/';
		appendGuiNumber(text, position.y);
		text << '/';
		appendGuiNumber(text, position.z);
	}

	/**
	 * @brief Refreshes the one-second memory sample and appends each debug value on its own line.
	 *
	 * A zero memory reading is formatted as unavailable. The camera coordinates
	 * are truncated to two decimal places before formatting. The completed
	 * framework String replaces the label text once after all fields are appended. Values are
	 * ordered as FPS, RAM, optional BuildPart controls, camera speed, and camera position.
	 */
	void GUI::frame()
	{
		DSE_WATCH();
		const float dt = glm::max(getTargetFrameDelay(), 0.0f);
		updateMessages(dt);
		memory_sample_timer += dt;
		if(memory_sample_timer >= 1.0f)
		{
			memory_sample_bytes = getProcessMemoryUsageBytes();
			memory_sample_timer = 0.0f;
		}

		const glm::dvec3 camera_position = truncateTo100(world.getCameraWorldPosition());
		ControllerSystem& controller = world.controller_system;
		uint32 selected_unit_count = 0;
		uint32 health_sample_count = 0;
		double total_health_percent = 0.0;
		for(const SelectedUnitRef& reference : controller.selected_unit_refs)
		{
			if(!world.point_data.isPointComponentSlotActive(reference.slot_id)) continue;
			const PointComponentSlot& slot = world.point_data.getPointComponentSlot(reference.slot_id);
			if(slot.generation_id != reference.generation_id
				|| slot.type_id != PointComponentTypeId::human
				|| !world.point_data.isPointActive(slot.point_id)
				|| !world.chunk_system.isChunkActive(slot.chunk_id)) continue;
			const HumanComponent& human = *static_cast<const HumanComponent*>(slot.getData());
			if(human.faction_id != base_faction_ids::player || human.hasDeathTimestamp()) continue;
			++selected_unit_count;
			const DamageableComponent* damageable = findDamageable(world, slot.chunk_id, slot.point_id);
			if(damageable == nullptr) continue;
			const float maximum_health = getDamageableDefinition(damageable->definition_id).max_health;
			if(maximum_health <= 0.0f) continue;
			total_health_percent += glm::clamp(damageable->getHealth() / maximum_health, 0.0f, 1.0f) * 100.0;
			++health_sample_count;
		}
		selected_units_panel.flags.set(GUIElementFlags::visible, selected_unit_count > 0);
		if(selected_unit_count > 0)
		{
			selected_units_label.text.clear();
			formatAppend(selected_units_label.text, "UNITS: %\nAVG HP: ", selected_unit_count);
			if(health_sample_count > 0)
			{
				formatAppend(selected_units_label.text, "%", static_cast<uint64>(
					total_health_percent / health_sample_count + 0.5));
				selected_units_label.text << '%';
			}
			else selected_units_label.text << "--%";
		}
		if(getKeyboardState(keyboard_buttons::g) == button_states::press)
		{
			controller.tool_catalog_open = !controller.tool_catalog_open;
			refreshToolCatalog();
		}
		setCursorCaptured(!controller.tool_catalog_open);
		catalog_panel.flags.set(GUIElementFlags::visible, controller.tool_catalog_open);
		for(uint32 index = 0; index < hotbar_column_count * hotbar_row_count; ++index)
		{
			const ToolCatalogEntryId entry_id = controller.hotbar_entry_ids[index];
			hotbar_item_labels[index].text = entry_id == no_tool_catalog_entry
				? "" : getToolCatalogEntry(entry_id).short_name;
			hotbar_slots[index].border_draw_style = index == controller.selected_hotbar_slot
				? &hotbar_selected_slot_border_style
				: &hotbar_slot_border_style;
		}
		String text;
		formatAppend(text, "FPS:%\n", fps_counter.getValue());
		formatAppend(text, "MODE:%\n", controller.mode == PlayerMode::god ? "GOD" : "COMMANDER");
		const size_t whole_mebibytes = memory_sample_bytes / mib(1);
		const size_t hundredths_of_mebibyte = (memory_sample_bytes % mib(1)) * 100 / mib(1);
		if(memory_sample_bytes == 0)
			text << "RAM: unavailable\n";
		else
		{
			formatAppend(text, "RAM:%.", whole_mebibytes);
			if(hundredths_of_mebibyte < 10) text << '0';
			formatAppend(text, "% MiB\n", hundredths_of_mebibyte);
		}

		if(controller.active_tool == ToolId::place
			&& (controller.selected_tool_action == ToolActionId::create_body
				|| controller.selected_tool_action == ToolActionId::create_slope))
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
		text << "camera pos:";
		appendGuiPosition(text, camera_position);
		debug_info_label.text = static_cast<String&&>(text);
	}
}
