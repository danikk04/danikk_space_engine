#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/tool_catalog.h>

namespace danikk_space_engine
{
	/** @brief Values needed to display one frame of the game interface. */
	struct GUIFrameData
	{
		/** @brief Camera position in world coordinates. */
		glm::dvec3 camera_position = glm::dvec3(0.0);
		/** @brief Camera speed in metres per second. */
		float camera_speed = 0.0f;
		/** @brief Signed height above the nearest planet surface, in metres. */
		double nearest_planet_height = 0.0;
		/** @brief Whether a planet was available for the nearest-surface height sample. */
		bool nearest_planet_height_available = false;
		/** @brief Average time in milliseconds for the most recent group of 50 generated meshes. */
		float average_mesh_generation_time_ms = 0.0f;
		/** @brief Whether at least one complete group of 50 mesh-generation samples is available. */
		bool average_mesh_generation_time_available = false;
		/** @brief Camera forward axis used to orient catalog previews. */
		vec3 preview_camera_front = vec3(0.0f, 0.0f, -1.0f);
		/** @brief Camera up axis used to orient catalog previews. */
		vec3 preview_camera_up = vec3(0.0f, 1.0f, 0.0f);
		/** @brief Vertical field of view used to scale catalog previews. */
		float preview_vertical_fov_degrees = 90.0f;
		/** @brief True when the player is in Commander mode. */
		bool commander_mode = false;
		/** @brief Whether the tool catalog is open. */
		bool tool_catalog_open = false;
		/** @brief Tool assigned to the selected hotbar slot. */
		ToolId active_tool = ToolId::none;
		/** @brief Action assigned to the selected hotbar slot. */
		ToolActionId selected_tool_action = ToolActionId::none;
		/** @brief BuildPart dimensions shown while placing a body. */
		vec3 build_part_size = vec3(1.0f);
		/** @brief BuildPart dimension axis currently adjusted. */
		uint8 build_part_size_axis = 2;
		/** @brief Quarter-turn count used for BuildPart placement. */
		uint8 build_part_rotation_step = 0;
		/** @brief Selected hotbar index, in row-major order. */
		uint32 selected_hotbar_slot = 0;
		/** @brief Catalog entries assigned to the 36 hotbar slots. */
		ToolCatalogEntryId hotbar_entry_ids[36]{};
		/** @brief Number of currently selected living allied units. */
		uint32 selected_unit_count = 0;
		/** @brief Average health of selected units, as a percentage. */
		float selected_unit_average_health_percent = 0.0f;
		/** @brief True when at least one selected unit contributed a health value. */
		bool selected_unit_health_available = false;
	};

	/** @brief An input requested by the GUI for the application/controller to apply. */
	enum class GUIActionType : uint8
	{
		toggle_tool_catalog,
		close_tool_catalog,
		select_tool_catalog_entry,
		select_hotbar_slot
	};

	/** @brief A GUI action and its optional catalog-entry or hotbar-slot value. */
	struct GUIAction
	{
		GUIActionType type = GUIActionType::toggle_tool_catalog;
		ToolCatalogEntryId catalog_entry_id = no_tool_catalog_entry;
		uint32 hotbar_slot = 0;
	};

	/**
	 * @brief Owns the debug overlay, selected-unit summary, message list, hotbar, and tool catalog.
	 *
	 * The GUI text reports FPS, process memory, BuildPart size when applicable, camera speed,
	 * signed height above the nearest planet, and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		/** @brief One player-facing message and its remaining display lifetime. */
		struct Message
		{
			String text;
			float remaining_seconds = 10.0f;
		};

		/** @brief Messages shown newest-first in the screen's top-right corner. */
		DynamicArray<Message> messages;
		/** @brief Multiline widget displaying the active message list. */
		GUIElement messages_label;
		/** @brief Shared bitmap text style for player-facing messages. */
		TextStyle messages_text_style;
		/** @brief Number of columns in the visual build hotbar. */
		static constexpr uint32 hotbar_column_count = 12;
		/** @brief Number of rows in the visual build hotbar. */
		static constexpr uint32 hotbar_row_count = 3;
		/** @brief Capacity for category-tab widgets; catalog data remains table-driven. */
		static constexpr uint32 catalog_category_capacity = 16;
		/** @brief Capacity for detail-cell widgets; catalog data remains table-driven. */
		static constexpr uint32 catalog_item_capacity = 64;
		/** @brief Empty toolbar slot elements, laid out row by row from the bottom. */
		GUIElement hotbar_slots[hotbar_column_count * hotbar_row_count];
		/** @brief Small keyboard shortcut labels drawn above each slot's background. */
		GUIElement hotbar_slot_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Compact item names displayed inside assigned hotbar slots. */
		GUIElement hotbar_item_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Summary panel for the currently selected allied units. */
		GUIElement selected_units_panel;
		/** @brief Selected unit count and average health percentage. */
		GUIElement selected_units_label;
		/** @brief Text style for the selected unit summary. */
		TextStyle selected_units_text_style;
		/** @brief Shared dark-gray fill style for every empty toolbar slot. */
		DrawStyle hotbar_slot_fill_style;
		/** @brief Shared thin gray outline style for every toolbar slot. */
		BorderDrawStyle hotbar_slot_border_style;
		/** @brief Brighter gray outline style for the currently selected toolbar slot. */
		BorderDrawStyle hotbar_selected_slot_border_style;
		/** @brief Shared small-font style for toolbar keyboard shortcut labels. */
		TextStyle hotbar_slot_text_style;
		/** @brief Shared small-font style for assigned hotbar item names. */
		TextStyle hotbar_item_text_style;
		/** @brief Modal tool catalog opened by the G key. */
		GUIElement catalog_panel;
		/** @brief Button that closes the catalog window. */
		GUIElement catalog_close_button;
		/** @brief Text shown inside the catalog close button. */
		GUIElement catalog_close_label;
		/** @brief Category tab backgrounds, sized from the category definition table. */
		Array<GUIElement, catalog_category_capacity> catalog_category_buttons;
		/** @brief Category tab text, sized from the category definition table. */
		Array<GUIElement, catalog_category_capacity> catalog_category_labels;
		/** @brief Detail entry buttons, sized from the tool catalog table. */
		Array<GUIElement, catalog_item_capacity> catalog_item_buttons;
		/** @brief Compact names displayed inside catalog command cells. */
		Array<GUIElement, catalog_item_capacity> catalog_item_labels;
		/** @brief Shared text style for catalog labels. */
		TextStyle catalog_text_style;
		/** @brief Fill and outline styles used by the catalog window and buttons. */
		DrawStyle catalog_panel_style;
		DrawStyle catalog_button_style;
		DrawStyle catalog_selected_button_style;
		BorderDrawStyle catalog_border_style;
		/** @brief Tool category currently shown in the menu. */
		ToolCategoryId selected_tool_category = 0;
		/** @brief Engine GUI element used to display the game's multiline debug readout. */
		GUIElement debug_info_label;
		/** @brief Most recently sampled process memory usage in bytes; zero means unavailable. */
		size_t memory_sample_bytes = 0;
		/** @brief Elapsed frame time since the previous process-memory sample. */
		float memory_sample_timer = 1.0f;
		/** @brief Latest app-provided values used to draw the GUI and catalog previews. */
		GUIFrameData frame_data;
		/** @brief GUI input requests waiting for the app/controller to process. */
		DynamicArray<GUIAction> pending_actions;

		/** @brief Refreshes visible commands and category highlighting in the tool menu. */
		void refreshToolCatalog();
		/** @brief Adds an application-facing action to the pending queue. */
		void queueAction(GUIActionType type, ToolCatalogEntryId entry_id = no_tool_catalog_entry,
			uint32 hotbar_slot = 0);
		/** @brief Ages messages, removes expired entries, and refreshes the visible list. */
		void updateMessages(float dt);
	public:

		/**
		 * @brief Configures the debug label and positions the empty three-row tool hotbar.
		 *
		 * The debug label uses the shared bitmap font and 0.014 screen-relative character dimensions.
		 * The hotbar contains twelve columns and three rows of gray slots, centered near the bottom
		 * edge. Its bottom row shows number symbols, middle row shows Shift shortcuts, and top row
		 * shows Ctrl shortcuts. The tool catalog uses All, Tools, Parts, and Human categories.
		 */
		void define();

		/**
		 * @brief Adds the debug overlay, hotbar, and tool catalog hierarchy to the engine GUI root.
		 * Call after define() so parent/child relationships and callbacks are registered.
		 */
		void compose();

		/**
		 * @brief Refreshes debug text, hotbar selection, catalog visibility, and sampled memory.
		 *
		 * Samples process memory when the accumulated target-frame delay reaches one second, then
		 * builds one multiline string containing FPS, RAM in MiB, relevant BuildPart controls,
		 * camera speed in m/s, signed nearest-planet height, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. The supplied frame snapshot controls the hotbar, catalog visibility,
		 * player mode, placement readout, and selected-unit summary. GUI input is queued as actions for
		 * the application to apply to the controller.
		 * @param data Values collected by the application for this GUI frame.
		 */
		void frame(const GUIFrameData& data);

		/** @brief Returns queued actions for the app/controller to process. */
		const DynamicArray<GUIAction>& getPendingActions() const { return pending_actions; }
		/** @brief Removes all actions already processed by the app/controller. */
		void clearPendingActions() { pending_actions.clear(); }

		/**
		 * @brief Adds a player-facing message at the top of the on-screen list.
		 * @param text Message text; the message remains visible for ten seconds.
		 */
		void addMessage(const String& text);
	};

	extern GUI gui;
}
