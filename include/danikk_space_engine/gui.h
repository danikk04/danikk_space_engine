#pragma once

#include <danikk_engine/gui.h>
#include <danikk_engine/fps.h>
#include <danikk_engine/style.h>
#include <danikk_space_engine/default.h>
#include <danikk_space_engine/build_catalog.h>

namespace danikk_space_engine
{

	/**
	 * @brief Owns the debug overlay, 3x12 hotbar, and modal build-detail catalog.
	 *
	 * The GUI text reports FPS, process memory, active tool/action, camera speed,
	 * and camera position. Process memory is sampled at roughly one-second
	 * intervals; a zero reading is displayed as unavailable.
	 */
	class GUI
	{
		/** @brief Number of columns in the visual build hotbar. */
		static constexpr uint32 hotbar_column_count = 12;
		/** @brief Number of rows in the visual build hotbar. */
		static constexpr uint32 hotbar_row_count = 3;
		/** @brief Maximum number of items shown in one catalog category. */
		static constexpr uint32 catalog_item_capacity = 2;
		/** @brief Empty toolbar slot elements, laid out row by row from the bottom. */
		GUIElement hotbar_slots[hotbar_column_count * hotbar_row_count];
		/** @brief Small keyboard shortcut labels drawn above each slot's background. */
		GUIElement hotbar_slot_labels[hotbar_column_count * hotbar_row_count];
		/** @brief Compact item names displayed inside assigned hotbar slots. */
		GUIElement hotbar_item_labels[hotbar_column_count * hotbar_row_count];
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
		/** @brief Modal category-and-item picker opened by the G key. */
		GUIElement catalog_panel;
		/** @brief Button that closes the catalog window. */
		GUIElement catalog_close_button;
		/** @brief Text shown inside the catalog close button. */
		GUIElement catalog_close_label;
		/** @brief Category tab backgrounds. */
		GUIElement catalog_category_buttons[2];
		/** @brief Category tab text. */
		GUIElement catalog_category_labels[2];
		/** @brief Detail entry buttons for the active category. */
		GUIElement catalog_item_buttons[catalog_item_capacity];
		/** @brief Shared text style for catalog labels. */
		TextStyle catalog_text_style;
		/** @brief Fill and outline styles used by the catalog window and buttons. */
		DrawStyle catalog_panel_style;
		DrawStyle catalog_button_style;
		DrawStyle catalog_selected_button_style;
		BorderDrawStyle catalog_border_style;
		/** @brief Category currently shown in the catalog item list. */
		BuildCategoryId selected_build_category = BuildCategoryId::structural;
		/** @brief Detail selected for assignment to a hotbar slot, or none. */
		BuildDefinitionId pending_build_definition_id = build_definition_ids::none;
		/** @brief Engine GUI element used to display the game's multiline debug readout. */
		GUIElement debug_info_label;
		/** @brief Most recently sampled process memory usage in bytes; zero means unavailable. */
		size_t memory_sample_bytes = 0;
		/** @brief Elapsed frame time since the previous process-memory sample. */
		float memory_sample_timer = 1.0f;

		/** @brief Refreshes visible catalog entries and category highlighting. */
		void refreshBuildCatalog();
		/** @brief Assigns the pending definition to one clicked hotbar slot. */
		void assignBuildDefinitionToSlot(uint32 slot_index);
	public:

		/**
		 * @brief Configures the debug label and positions the empty three-row build hotbar.
		 *
		 * The debug label uses the shared bitmap font and 0.014 screen-relative character dimensions.
		 * The hotbar contains twelve columns and three rows of gray slots, centered near the bottom
		 * edge. Its bottom row shows number symbols, middle row shows Shift shortcuts, and top row
		 * shows Ctrl shortcuts. The build catalog is configured with Structural and Weapons categories.
		 */
		void define();

		/**
		 * @brief Adds the debug overlay, hotbar, and build catalog hierarchy to the engine GUI root.
		 * Call after define() so parent/child relationships and callbacks are registered.
		 */
		void compose();

		/**
		 * @brief Refreshes debug text, hotbar selection, catalog visibility, and sampled memory.
		 *
		 * Samples process memory when the accumulated target-frame delay reaches one second, then
		 * builds one multiline string in this order: FPS, RAM in MiB, active tool, selected action,
		 * camera speed in m/s, and camera position truncated to hundredths. A zero memory sample is
		 * shown as unavailable. Pressing G toggles the modal build catalog; selected slot borders are
		 * updated from controller state. Rendering is driven by the surrounding engine frame loop.
		 */
		void frame();
	};

	extern GUI gui;
}
